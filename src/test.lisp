
(ql:quickload :cffi)
(ql:quickload :prove)

(defpackage clcxx/test
  (:use :cl
        :prove
        :cffi
        ))

(in-package :clcxx/test)

(require :cffi-libffi)

(define-foreign-library libclcxx
  (:darwin "libclcxx.dylib")
  (:unix (:or "~/common-lisp/programs/hack/clcxx/build/lib/libclcxx.so" "libclcxx.so"))
  (t (:default "libclcxx")))

(use-foreign-library libclcxx)

;; extern "C" typedef union {
;;   float Float;
;;   double Double;
;;   long double LongDouble;
;; } ComplexType;
(defcunion complex-type
  (f :float)
  (d :double))

;; ;; extern "C" typedef struct {
;; ;;   ComplexType real;
;; ;;   ComplexType imag;
;; ;; } LispComplex;
(defcstruct cxx-complex
  (real (:union complex-type))
  (imag (:union complex-type)))

;; extern "C" typedef struct {
;;   char *name;
;;   char *super_classes;
;;   char *slot_names;
;;   char *slot_types;
;;   void *constructor;
;;   void *destructor;
;; } ClassInfo;
(defcstruct class-info
  (name :string)
  (super-classes :string)
  (slot-names :string)
  (slot-types :string)
  (constructor :pointer)
  (destructor :pointer))
;; extern "C" typedef struct {
;;   char *name;
;;   bool method_p;
;;   char *class_obj;
;;   void *thunk_func;
;;   size_t index;
;;   char *arg_types;
;;   char *return_type;
;; } FunctionInfo;
(defcstruct function-info
  (name :string)
  (method-p :bool)
  (class-obj :string)
  (thunc-func :pointer)
  (index :uint16)
  (arg-types :string)
  (return-type :string))

;; extern "C" typedef struct {
;;   char *name;
;;   char *value;
;; } ConstantInfo;
(defcstruct constant-info
  (name :string)
  (value :string))

;; typedef union {
;;   clcxx::FunctionInfo Func;
;;   clcxx::ClassInfo Class;
;;   clcxx::ConstantInfo Const;
;; } MetaData;
(defcunion Meta-Data
  (func (:struct function-info))
  (class (:struct class-info))
  (const (:struct constant-info)))

;; inline void lisp_error(const char *error)
(defcallback lisp-error :void ((err :string))
  (format t "Caught error: ~a~%" err))

;; void send_data(MetaData *M, uint8_t n)
(defcallback reg-data :void ((meta-ptr :pointer) (type :uint8))
  (ecase type
    (0 (print "class")
       (print meta-ptr)
       (with-foreign-slots ((name super-classes slot-names slot-types constructor destructor) meta-ptr (:struct class-info))
         (format t "name:~A super-classes:~A slot-names:~A slot-types:~A constructor:~A destructor:~A~%"
                 name super-classes slot-names slot-types  constructor destructor)))
    (1 (print "constant")
       (print meta-ptr)
       (with-foreign-slots ((name value) meta-ptr (:struct constant-info))
         (format t "name:~A value:~A~%"
                 name value)))
    (2 (print "function")
       (print meta-ptr)
       (with-foreign-slots ((name method-p class-obj thunc-func index arg-types return-type) meta-ptr (:struct function-info))
         (format t "name:~A method-p:~A class-obj:~A thunc-func:~A index:~A arg-types:~A return-type:~A~%"
                 name method-p class-obj thunc-func index arg-types return-type)))))

;; void remove_package(char *pack_name)
(defcfun ("remove_package" remove-package) :void
  (name :string))

;; void clcxx_init(void (*error_handler)(char *),
;;                      void (*reg_data_callback)(MetaData *, uint8_t))
(defcfun ("clcxx_init" init) :void
  (err-callback :pointer)
  (reg-data-callback :pointer))

;; void register_lisp_package(const char *cl_pack,
;;                                  void (*regfunc)(clcxx::Package &))
(defcfun ("register_package" register-package) :void
  (name :string)
  (pack-ptr :pointer))

;; Init. clcxx
(init (callback lisp-error) (callback reg-data))

(defun add-package (pack-name func-name)
  "Register lisp package with pack-name 
            from func-name defined in CXX lib"
  (declare (type string pack-name func-name))
  (register-package pack-name (foreign-symbol-pointer func-name)))




