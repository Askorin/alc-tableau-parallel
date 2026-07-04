#! /opt/racket/bin/racket

;;#! /usr/bin/env racket

#lang racket

(require math/number-theory)
(require racket)
(require racket/string)


;;1
;; <Declaration>
;;     <Class IRI="#x5"/>
;; </Declaration>

;;2
;; <SubClassOf>
;;     <Class IRI="#x5"/>
;;     <Class IRI="#x0"/>
;; </SubClassOf>

;;3
;; <Declaration>
;;     <ObjectProperty IRI="#r1"/>
;; </Declaration>

;;4
;; <SubObjectPropertyOf>
;;     <ObjectProperty IRI="#r1"/>
;;     <ObjectProperty IRI="#r0"/>
;; </SubObjectPropertyOf>

;;5
;; <SubClassOf>
;;     <ObjectUnionOf>
;;         <ObjectAllValuesFrom>
;;             <ObjectProperty IRI="#r1"/>
;;             <ObjectIntersectionOf>
;;                 <Class IRI="#x1"/>
;;                 <Class IRI="#x2"/>
;;             </ObjectIntersectionOf>
;;         </ObjectAllValuesFrom>
;;         <ObjectAllValuesFrom>
;;             <ObjectProperty IRI="#r3"/>
;;             <ObjectIntersectionOf>
;;                 <Class IRI="#x3"/>
;;                 <Class IRI="#x4"/>
;;             </ObjectIntersectionOf>
;;         </ObjectAllValuesFrom>
;;     </ObjectUnionOf>
;;     <ObjectIntersectionOf>
;;         <ObjectSomeValuesFrom>
;;             <ObjectProperty IRI="#r2"/>
;;             <ObjectUnionOf>
;;                 <Class IRI="#x2"/>
;;                 <Class IRI="#x3"/>
;;             </ObjectUnionOf>
;;         </ObjectSomeValuesFrom>
;;         <ObjectSomeValuesFrom>
;;             <ObjectProperty IRI="#r4"/>
;;             <ObjectUnionOf>
;;                 <Class IRI="#x4"/>
;;                 <Class IRI="#x5"/>
;;             </ObjectUnionOf>
;;         </ObjectSomeValuesFrom>
;;     </ObjectIntersectionOf>
;; </SubClassOf>

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define iniv 1)
(define mav 128)
(define max-range 9)

(define a1 "<Declaration><Class IRI=\"#x")
(define b1 "\"/></Declaration>")
(define a2 "<SubClassOf><Class IRI=\"#x")
(define b2 "\"/><Class IRI=\"#x0\"/></SubClassOf>")
(define a3 "<Declaration><ObjectProperty IRI=\"#r")
(define b3 "\"/></Declaration>")
(define a4 "<SubObjectPropertyOf><ObjectProperty IRI=\"#r")
(define b4 "\"/><ObjectProperty IRI=\"#r0\"/></SubObjectPropertyOf>")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


(define gedec
  (λ (i m s)
	  (if (<= i m)
		  ((λ (k)
			   (gedec (+ k 1) m
					  (string-append
					   s
					   a1
					   (number->string k)
					   b1
					   a2
					   (number->string k)
					   b2
					   a3
					   (number->string k)
					   b3
					   a4
					   (number->string k)
					   b4))) i)
		  ((λ (k)
			   (string-append
				s
				a1
				(number->string k)
				b1
				a2
				(number->string k)
				b2)) i))))

  (define gegcisubi
	(λ (i)
	  (string-append
	   "<ObjectAllValuesFrom><ObjectProperty IRI=\"#r"
	   (number->string i)
	   "\"/><ObjectIntersectionOf><Class IRI=\"#x"
	   (number->string i)
	   "\"/><Class IRI=\"#x"
	   (number->string (+ i 1))
	   "\"/></ObjectIntersectionOf></ObjectAllValuesFrom>")))

(define (foo i m s)
  (if (< i m)
	  (foo (+ i 1) m (+ i s))
	  ((λ (k) (+ 1 k)(+ 2 k)) s)))

(define gegcisupi
  (λ (i)
	  (string-append
	   "<ObjectSomeValuesFrom><ObjectProperty IRI=\"#r"
	   (number->string i)
	   "\"/><ObjectUnionOf><Class IRI=\"#x"
	   (number->string i)
	   "\"/><Class IRI=\"#x"
	   (number->string (+ i 1))
	   "\"/></ObjectUnionOf></ObjectSomeValuesFrom>")))

;; (define consubis
;;   (λ (i m subis)
;; 	  (if (<= i m)
;; 		  (consubis (+ i 1) m (cons (gegcisubi i) subis))
;; 		  subis)))

;; (define consupis
;;   (λ (i m supis)
;; 	  (if (<= i m)
;; 		  (consupis (+ i 1) m (cons (gegcisupi i) supis))
;; 		  supis)))

(define consgciis
  (λ (i m subs sups)
	  (if (<= i m)
		  (if (= (modulo i 2) 0)
			  (consgciis (+ i 1) m subs (cons (gegcisupi i) sups))
			  (consgciis (+ i 1) m (cons (gegcisubi i) subs) sups))
		  (cons subs sups))))

(define gegci
  (λ (i m)
	  (let ((gciis (consgciis i m '() '())))
		(string-append 
		 "<SubClassOf>"
		 "<ObjectUnionOf>" (foldl string-append "</ObjectUnionOf>" (car gciis))
		 "<ObjectIntersectionOf>" (foldl string-append "</ObjectIntersectionOf>" (cdr gciis))
		 "</SubClassOf>"))))

(define pin
  (λ (i m o)
	  (display (gedec i m "") o)
	  (display (gegci i m) o)))

(define pin2
  (λ (i m)
	  (string-append (gedec i m "") (gegci i m))))

(define make-one-puppet
  (λ (t s d n e)
	  (let ((o (open-output-file (string-append n "_" (number->string e) ".owl") #:mode 'text #:exists 'replace)))
		(display (string-replace t s d) o)
		(close-output-port o))))

(define make-puppets
  (λ (t s n r)
	  (map
	   (λ (c)
		   (make-one-puppet t s (pin2 iniv c) n c))
	   (map (make-fibonacci 2 3) (range r)))))
	  

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; (define out (open-output-file "data"))
;; (pin iniv mav out)
;; (close-output-port out)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(define template (file->string "tea_for_testing_trace.template" #:mode 'text))
(define source "<!--ggccii-->")
(define puppet-name "tea_for_testing_trace")
(make-puppets template source puppet-name max-range)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define fooba
  (lambda ()
	(if (= 1 1)
		((lambda ()
		   (display "good")
		   (display "noon")))
		(display "haha"))))
