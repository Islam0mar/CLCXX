
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
;;   void *thunk_ptr;
;;   void *func_ptr;
;;   char *arg_types;
;;   char *return_type;
;; } FunctionInfo;
(defcstruct function-info
  (name :string)
  (method-p :bool)
  (class-obj :string)
  (thunc-ptr :pointer)
  (func-ptr :pointer)
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
       (with-foreign-slots ((name super-classes slot-names slot-types constructor destructor) meta-ptr (:struct class-info))
         (format t "name:~A super-classes:~A slot-names:~A slot-types:~A constructor:~A destructor:~A~%"
                 name super-classes slot-names slot-types  constructor destructor)))
    (1 (print "constant")
       (with-foreign-slots ((name value) meta-ptr (:struct constant-info))
         (format t "name:~A value:~A~%"
                 name value)))
    (2 (print "function")
       (print (parse-function meta-ptr))
       (print "~%")
       (with-foreign-slots ((name method-p class-obj thunc-ptr func-ptr arg-types return-type) meta-ptr (:struct function-info))
         (format t "name:~A method-p:~A class-obj:~A thunc-ptr:~A func-ptr:~A arg-types:~A return-type:~A~%"
                 name method-p class-obj thunc-ptr func-ptr arg-types return-type)))))

(defun split-string-by (string char)
  "Returns a list of substrings of string
divided by ONE + each.
;; Note: Two consecutive pluses will be seen as
;; if there were an empty string between them."
  (declare (type string string))
  (declare (type character char))
  (remove ""
          (loop for i = 0 then (1+ j)
             as j = (position char string :start i)
             collect (subseq string i j)
             while j) :test #'equal))

(defun get-parenthes-string (string)
  "Returns a string within first (...) or nil"
  (declare (type string string))
  (if (not (position #\( string)) (return-from get-parenthes-string))
  (loop
     for i from (position #\( string) below (length string)
     for j = (position #\( string) then j
     for k = (char string i) then (char string i)
     for l = 0 then l     
     do
       (case k
         (#\( (setf l (1+ l)))
         (#\) (setf l (1- l))))
       (if (= l 0)
           (return (subseq string j (1+ i))))))

(defun remove-string (rem-string full-string &key from-end (test #'eql)
                      test-not (start1 0) end1 (start2 0) end2 key)
  "returns full-string with rem-string removed"
  (let ((subst-point (search rem-string full-string 
                             :from-end from-end
                             :test test :test-not test-not
                             :start1 start1 :end1 end1
                             :start2 start2 :end2 end2 :key key)))
    (if subst-point
        (concatenate 'string
                     (subseq full-string 0 subst-point)
                     (subseq full-string (+ subst-point (length rem-string))))
        full-string)))

(defun symbols-list (arg-types)
  "Return a list of symbols '(V0 V1 V2 V3 ...) 
   representing thenumber of args"
  (if arg-types (loop for i in (split-string-by arg-types #\+)
                   for j from 0
                   collect (intern (concatenate 'string "V" (write-to-string j))))))

(defun compound-type-list (type &optional (array-p nil))
    (declare (type string type))
    (let* ((string (string-trim "(" type))
           (str (get-parenthes-string string))
           (lst (append (split-string-by
                         (string-trim ")"
                                      (remove-string str string))
                         #\space) (list str))))
      (if str (progn (if array-p (rotatef (nth 1 lst) (nth 2 lst)))
                     lst)
          (split-string-by (string-trim ")" string) #\space))))

(defun parse-type (type)
  "Returns cffi-type as a keyword
      or list of keywords"
  (declare (type string type))
  (if (equal (subseq type 0 1) "(")
      ;; compound type
      (cond ((if (>= (length type) 7) (equal (subseq type 0 7) "(:array") nil)
             (mapcar #'parse-type (compound-type-list type t)))
            ((if (>= (length type) 9) (equal (subseq type 0 9) "(:pointer") nil)
             (mapcar #'parse-type (compound-type-list type)))
            ((if (>= (length type) 11) (equal (subseq type 0 11) "(:reference") nil)
             (mapcar #'parse-type (compound-type-list type)))
            ((if (>= (length type) 9) (equal (subseq type 0 9) "(:complex") nil)
             (mapcar #'parse-type (compound-type-list type)))
            ((if (>= (length type) 8) (equal (subseq type 0 8) "(:struct") nil)
             (mapcar #'parse-type (compound-type-list type)))
            ((if (>= (length type) 7) (equal (subseq type 0 7) "(:class") nil)
             (mapcar #'parse-type (compound-type-list type)))
            (t (error "Unkown type : ~S" type)))
      ;; simple type
      (read-from-string type)))

(defun cffi-type (type)
  "Returns cffi-type as a keyword
      or list of keywords"
  (declare (type string type))
  (let ((parsed-type (parse-type type)))
    (if (listp parsed-type) (ecase (first parsed-type)
                              (:array (append (list (second parsed-type)) (list :count)  (list (third parsed-type))))
                              (:complex :pointer)
                              (:pointer :pointer)
                              (:reference :pointer)
                              (:class :pointer)
                              (:struct (second parsed-type)))
        parsed-type)))


(defun parse-args (arg-types &optional (list-p t))
  "return a list of function inputs"
  (if list-p (loop
                for i in (split-string-by arg-types #\+)
                for sym in (symbols-list arg-types)
                as type = (cffi-type i) then (cffi-type i)
                append (if (listp type) `( ,@type ,sym)
                            `( ,type ,sym)))
      (let ((type (cffi-type arg-types)))
        (if (listp type) type
            (list type)))))
       
(defun parse-function (meta-ptr)
  (with-foreign-slots ((name method-p class-obj thunc-ptr func-ptr arg-types return-type) meta-ptr (:struct function-info))
    (if (not method-p)
        `(defun ,name ,(symbols-list arg-types)
           ;; (name :string)
           ;; (pack-ptr :pointer)
           ,(if arg-types `(cffi:foreign-funcall-pointer
                            ,thunc-ptr
                            ,@(parse-args arg-types)
                            ,@(parse-args return-type nil))
                `(cffi:foreign-funcall-pointer
                  ,thunc-ptr
                  ,@(parse-args return-type nil)))))))

    
;; bool remove_package(char *pack_name)
(defcfun ("remove_package" remove-package) :bool
  (name :string))

;; bool clcxx_init(void (*error_handler)(char *),
;;                      void (*reg_data_callback)(MetaData *, uint8_t))
(defcfun ("clcxx_init" init) :bool
  (err-callback :pointer)
  (reg-data-callback :pointer))

;; bool register_lisp_package(const char *cl_pack,
;;                                  void (*regfunc)(clcxx::Package &))
(defcfun ("register_package" register-package) :bool
  (name :string)
  (pack-ptr :pointer))

;; Init. clcxx
(init (callback lisp-error) (callback reg-data))

(defun add-package (pack-name func-name)
  "Register lisp package with pack-name 
            from func-name defined in CXX lib"
  (declare (type string pack-name func-name))
  (let ((curr-pack *package*))
    (make-package pack-name :use '("COMMON-LISP"))
    (use-package pack-name)
    (register-package pack-name (foreign-symbol-pointer func-name))
    (use-package curr-pack)))




