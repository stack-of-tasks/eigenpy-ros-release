/// Copyright (c) 2016-2022 CNRS INRIA
/// This file was taken from Pinocchio (header
/// <pinocchio/bindings/python/utils/std-vector.hpp>)
///

#ifndef __eigenpy_utils_std_vector_hpp__
#define __eigenpy_utils_std_vector_hpp__

#include <boost/mpl/if.hpp>
#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <iterator>
#include <string>
#include <vector>

#include "eigenpy/config.hpp"
#include "eigenpy/copyable.hpp"
#include "eigenpy/eigen-to-python.hpp"
#include "eigenpy/pickle-vector.hpp"
#include "eigenpy/registration.hpp"

namespace eigenpy {
// Forward declaration
template <typename vector_type, bool NoProxy = false>
struct StdContainerFromPythonList;

namespace details {

/// \brief Check if a PyObject can be converted to an std::vector<T>.
template <typename T>
bool from_python_list(PyObject *obj_ptr, T *) {
  namespace bp = ::boost::python;

  // Check if it is a list
  if (!PyList_Check(obj_ptr)) return false;

  // Retrieve the underlying list
  bp::object bp_obj(bp::handle<>(bp::borrowed(obj_ptr)));
  bp::list bp_list(bp_obj);
  bp::ssize_t list_size = bp::len(bp_list);

  // Check if all the elements contained in the current vector is of type T
  for (bp::ssize_t k = 0; k < list_size; ++k) {
    bp::extract<T> elt(bp_list[k]);
    if (!elt.check()) return false;
  }

  return true;
}

template <typename vector_type, bool NoProxy>
struct build_list {
  static ::boost::python::list run(vector_type &vec) {
    namespace bp = ::boost::python;

    bp::list bp_list;
    for (size_t k = 0; k < vec.size(); ++k) {
      bp_list.append(boost::ref(vec[k]));
    }
    return bp_list;
  }
};

template <typename vector_type>
struct build_list<vector_type, true> {
  static ::boost::python::list run(vector_type &vec) {
    namespace bp = ::boost::python;

    typedef bp::iterator<vector_type> iterator;
    return bp::list(iterator()(vec));
  }
};

/// \brief Change the behaviour of indexing (method __getitem__ in Python).
/// This is suitable for container of Eigen matrix objects if you want to mutate
/// them.
template <typename Container>
struct overload_base_get_item_for_std_vector
    : public boost::python::def_visitor<
          overload_base_get_item_for_std_vector<Container> > {
  typedef typename Container::value_type value_type;
  typedef typename Container::value_type data_type;
  typedef size_t index_type;

  template <class Class>
  void visit(Class &cl) const {
    cl.def("__getitem__", &base_get_item);
  }

 private:
  static boost::python::object base_get_item(
      boost::python::back_reference<Container &> container, PyObject *i_) {
    namespace bp = ::boost::python;

    index_type idx = convert_index(container.get(), i_);
    typename Container::iterator i = container.get().begin();
    std::advance(i, idx);
    if (i == container.get().end()) {
      PyErr_SetString(PyExc_KeyError, "Invalid index");
      bp::throw_error_already_set();
    }

    typename bp::to_python_indirect<data_type &,
                                    bp::detail::make_reference_holder>
        convert;
    return bp::object(bp::handle<>(convert(*i)));
  }

  static index_type convert_index(Container &container, PyObject *i_) {
    namespace bp = boost::python;
    bp::extract<long> i(i_);
    if (i.check()) {
      long index = i();
      if (index < 0) index += (long)container.size();
      if (index >= long(container.size()) || index < 0) {
        PyErr_SetString(PyExc_IndexError, "Index out of range");
        bp::throw_error_already_set();
      }
      return (index_type)index;
    }

    PyErr_SetString(PyExc_TypeError, "Invalid index type");
    bp::throw_error_already_set();
    return index_type();
  }
};
}  // namespace details
}  // namespace eigenpy

namespace boost {
namespace python {

template <typename MatrixType>
struct extract_to_eigen_ref
    : converter::extract_rvalue<Eigen::Ref<MatrixType> > {
  typedef Eigen::Ref<MatrixType> RefType;

 protected:
  typedef converter::extract_rvalue<RefType> base;

 public:
  typedef RefType result_type;

  operator result_type() const { return (*this)(); }

  extract_to_eigen_ref(PyObject *o) : base(o) {}
  extract_to_eigen_ref(api::object const &o) : base(o.ptr()) {}
};

/// \brief Specialization of the boost::python::extract struct for references to
/// Eigen matrix objects.
template <typename Scalar, int Rows, int Cols, int Options, int MaxRows,
          int MaxCols>
struct extract<Eigen::Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols> &>
    : extract_to_eigen_ref<
          Eigen::Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols> > {
  typedef Eigen::Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols>
      MatrixType;
  typedef extract_to_eigen_ref<MatrixType> base;
  extract(PyObject *o) : base(o) {}
  extract(api::object const &o) : base(o.ptr()) {}
};

template <typename Derived>
struct extract<Eigen::MatrixBase<Derived> &>
    : extract_to_eigen_ref<Eigen::MatrixBase<Derived> > {
  typedef Eigen::MatrixBase<Derived> MatrixType;
  typedef extract_to_eigen_ref<MatrixType> base;
  extract(PyObject *o) : base(o) {}
  extract(api::object const &o) : base(o.ptr()) {}
};

template <typename Derived>
struct extract<Eigen::RefBase<Derived> &>
    : extract_to_eigen_ref<Eigen::RefBase<Derived> > {
  typedef Eigen::RefBase<Derived> MatrixType;
  typedef extract_to_eigen_ref<MatrixType> base;
  extract(PyObject *o) : base(o) {}
  extract(api::object const &o) : base(o.ptr()) {}
};

namespace converter {

template <typename Type, class Allocator>
struct reference_arg_from_python<std::vector<Type, Allocator> &>
    : arg_lvalue_from_python_base {
  typedef std::vector<Type, Allocator> vector_type;
  typedef vector_type &ref_vector_type;
  typedef ref_vector_type result_type;
  typedef extract<Type &> extract_type;

  reference_arg_from_python(PyObject *py_obj)
      : arg_lvalue_from_python_base(converter::get_lvalue_from_python(
            py_obj, registered<vector_type>::converters)),
        m_data(NULL),
        m_source(py_obj),
        vec_ptr(NULL) {
    if (result() != 0)  // we have found a lvalue converter
      return;

    // Check if py_obj is a py_list, which can then be converted to an
    // std::vector
    bool is_convertible =
        ::eigenpy::details::from_python_list(py_obj, (Type *)(0));
    if (!is_convertible) return;

    typedef ::eigenpy::StdContainerFromPythonList<vector_type> Constructor;
    Constructor::construct(py_obj, &m_data.stage1);

    void *&m_result = const_cast<void *&>(result());
    m_result = m_data.stage1.convertible;
    vec_ptr = reinterpret_cast<vector_type *>(m_data.storage.bytes);
  }

  result_type operator()() const {
    return ::boost::python::detail::void_ptr_to_reference(result(),
                                                          (result_type(*)())0);
  }

  ~reference_arg_from_python() {
    if (m_data.stage1.convertible == m_data.storage.bytes) {
      // Copy back the reference
      const vector_type &vec = *vec_ptr;
      list bp_list(handle<>(borrowed(m_source)));
      for (size_t i = 0; i < vec.size(); ++i) {
        typename extract_type::result_type elt = extract_type(bp_list[i]);
        elt = vec[i];
      }
    }
  }

 private:
  rvalue_from_python_data<ref_vector_type> m_data;
  PyObject *m_source;
  vector_type *vec_ptr;
};

}  // namespace converter
}  // namespace python
}  // namespace boost

namespace eigenpy {

///
/// \brief Register the conversion from a Python list to a std::vector
///
/// \tparam vector_type A std container (e.g. std::vector or std::list)
///
template <typename vector_type, bool NoProxy>
struct StdContainerFromPythonList {
  typedef typename vector_type::value_type T;
  typedef typename vector_type::allocator_type Allocator;

  /// \brief Check if obj_ptr can be converted
  static void *convertible(PyObject *obj_ptr) {
    namespace bp = boost::python;

    // Check if it is a list
    if (!PyList_Check(obj_ptr)) return 0;

    // Retrieve the underlying list
    bp::object bp_obj(bp::handle<>(bp::borrowed(obj_ptr)));
    bp::list bp_list(bp_obj);
    bp::ssize_t list_size = bp::len(bp_list);

    // Check if all the elements contained in the current vector is of type T
    for (bp::ssize_t k = 0; k < list_size; ++k) {
      bp::extract<T> elt(bp_list[k]);
      if (!elt.check()) return 0;
    }

    return obj_ptr;
  }

  /// \brief Allocate the std::vector and fill it with the element contained in
  /// the list
  static void construct(
      PyObject *obj_ptr,
      boost::python::converter::rvalue_from_python_stage1_data *memory) {
    namespace bp = boost::python;

    // Extract the list
    bp::object bp_obj(bp::handle<>(bp::borrowed(obj_ptr)));
    bp::list bp_list(bp_obj);

    void *storage =
        reinterpret_cast<
            bp::converter::rvalue_from_python_storage<vector_type> *>(
            reinterpret_cast<void *>(memory))
            ->storage.bytes;

    typedef bp::stl_input_iterator<T> iterator;

    // Build the std::vector
    new (storage) vector_type(iterator(bp_list), iterator());

    // Validate the construction
    memory->convertible = storage;
  }

  static void register_converter() {
    ::boost::python::converter::registry::push_back(
        &convertible, &construct, ::boost::python::type_id<vector_type>());
  }

  static ::boost::python::list tolist(vector_type &self) {
    return details::build_list<vector_type, NoProxy>::run(self);
  }
};

namespace internal {

template <typename T>
struct has_operator_equal
    : boost::mpl::if_<typename boost::is_base_of<Eigen::EigenBase<T>, T>::type,
                      has_operator_equal<Eigen::EigenBase<T> >,
                      boost::true_type>::type {};

template <typename T, class A>
struct has_operator_equal<std::vector<T, A> > : has_operator_equal<T> {};

template <>
struct has_operator_equal<bool> : boost::true_type {};

template <typename EigenObject>
struct has_operator_equal<Eigen::EigenBase<EigenObject> >
    : has_operator_equal<typename EigenObject::Scalar> {};

template <typename T, bool has_operator_equal_value = boost::is_base_of<
                          boost::true_type, has_operator_equal<T> >::value>
struct contains_algo;

template <typename T>
struct contains_algo<T, true> {
  template <class Container, typename key_type>
  static bool run(Container &container, key_type const &key) {
    return std::find(container.begin(), container.end(), key) !=
           container.end();
  }
};

template <typename T>
struct contains_algo<T, false> {
  template <class Container, typename key_type>
  static bool run(Container &container, key_type const &key) {
    for (size_t k = 0; k < container.size(); ++k) {
      if (&container[k] == &key) return true;
    }
    return false;
  }
};

template <class Container, bool NoProxy>
struct contains_vector_derived_policies
    : public ::boost::python::vector_indexing_suite<
          Container, NoProxy,
          contains_vector_derived_policies<Container, NoProxy> > {
  typedef typename Container::value_type key_type;

  static bool contains(Container &container, key_type const &key) {
    return contains_algo<key_type>::run(container, key);
  }
};
}  // namespace internal

struct EmptyPythonVisitor
    : public ::boost::python::def_visitor<EmptyPythonVisitor> {
  template <class classT>
  void visit(classT &) const {}
};

///
/// \brief Expose an std::vector from a type given as template argument.
///
/// \tparam T Type to expose as std::vector<T>.
/// \tparam Allocator Type for the Allocator in std::vector<T,Allocator>.
/// \tparam NoProxy When set to false, the elements will be copied when returned
/// to Python. \tparam EnableFromPythonListConverter Enables the conversion from
/// a Python list to a std::vector<T,Allocator>
///
/// \sa StdAlignedVectorPythonVisitor
///
template <class vector_type, bool NoProxy = false,
          bool EnableFromPythonListConverter = true>
struct StdVectorPythonVisitor
    : public ::boost::python::vector_indexing_suite<
          vector_type, NoProxy,
          internal::contains_vector_derived_policies<vector_type, NoProxy> >,
      public StdContainerFromPythonList<vector_type, NoProxy> {
  typedef typename vector_type::value_type value_type;
  typedef typename vector_type::allocator_type allocator_type;
  typedef StdContainerFromPythonList<vector_type, NoProxy>
      FromPythonListConverter;

  static void expose(const std::string &class_name,
                     const std::string &doc_string = "") {
    expose(class_name, doc_string, EmptyPythonVisitor());
  }

  template <typename VisitorDerived>
  static void expose(
      const std::string &class_name,
      const boost::python::def_visitor<VisitorDerived> &visitor) {
    expose(class_name, "", visitor);
  }

  template <typename VisitorDerived>
  static void expose(
      const std::string &class_name, const std::string &doc_string,
      const boost::python::def_visitor<VisitorDerived> &visitor) {
    namespace bp = boost::python;

    if (!register_symbolic_link_to_registered_type<vector_type>()) {
      bp::class_<vector_type> cl(class_name.c_str(), doc_string.c_str());
      cl.def(StdVectorPythonVisitor())

          .def(bp::init<size_t, const value_type &>(
              bp::args("self", "size", "value"),
              "Constructor from a given size and a given value."))
          .def(bp::init<const vector_type &>(bp::args("self", "other"),
                                             "Copy constructor"))

          .def("tolist", &FromPythonListConverter::tolist, bp::arg("self"),
               "Returns the std::vector as a Python list.")
          .def(visitor)
          .def("reserve", &vector_type::reserve,
               (bp::arg("self"), bp::arg("new_cap")),
               "Increase the capacity of the vector to a value that's greater "
               "or equal to new_cap.")
          .def_pickle(PickleVector<vector_type>())
          .def(CopyableVisitor<vector_type>());

      // Register conversion
      if (EnableFromPythonListConverter)
        FromPythonListConverter::register_converter();
    }
  }
};

/**
 * Expose std::vector for given matrix or vector sizes.
 */
void EIGENPY_DLLAPI exposeStdVector();

template <typename MatType>
void exposeStdVectorEigenSpecificType(const char *name) {
  typedef std::vector<MatType> VecMatType;
  std::string full_name = "StdVec_";
  full_name += name;
  StdVectorPythonVisitor<VecMatType, false>::expose(
      full_name.c_str(),
      details::overload_base_get_item_for_std_vector<VecMatType>());
}

}  // namespace eigenpy

#endif  // ifndef __eigenpy_utils_std_vector_hpp__
