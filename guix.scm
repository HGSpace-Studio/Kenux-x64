;; Guix Shepherd Service Definition
(use-modules (guix)
            (guix records)
            (ice-9 match)
            (srfi srfi-1)
            (srfi srfi-26)
            (srfi srfi-9)
            (srfi srfi-11))

(define-module (shepherd)
  #:use-module (ice-9 match)
  #:use-module (ice-9 control)
  #:use-module (srfi srfi-1)
  #:use-module (srfi srfi-26)
  #:use-module (srfi srfi-9)
  #:export (make-service
            service?
            service-name
            service-documentation
            service-proc
            service-requirement
            service-auto-start?
            service-auto-stop?
            service-running?
            service-value
            start-service
            stop-service
            restart-service
            reload-service
            status-service
            register-service
            unregister-service
            list-services
            run-shepherd))

;; Service record definition
(define-record-type <service>
  (make-service name documentation proc requirement auto-start? auto-stop? running? value)
  service?
  (name          service-name)
  (documentation service-documentation)
  (proc          service-proc)
  (requirement   service-requirement)
  (auto-start?   service-auto-start?)
  (auto-stop?    service-auto-stop?)
  (running?      service-running?)
  (value         service-value))

;; Service registry
(define service-registry '())

;; Helper procedures
(define (service-eq? s1 s2)
  (string=? (service-name s1) (service-name s2)))

;; Service management procedures
(define (make-service name doc proc #:requirement (requirement '()) 
                      #:auto-start? (auto-start? #t) 
                      #:auto-stop? (auto-stop? #t))
  "Create a new service record."
  (make-service name doc proc requirement auto-start? auto-stop? #f #f))

(define (start-service service)
  "Start a service."
  (when (service-running? service)
    (error (string-append "Service " (service-name service) " is already running")))
  
  ;; Check if requirements are satisfied
  (for-each (lambda (req)
              (let ((req-service (find (lambda (s) 
                                         (string=? (service-name s) req))
                                       service-registry)))
                (unless (and req-service (service-running? req-service))
                  (error (string-append "Requirement " req " not satisfied for service " 
                                        (service-name service))))))
            (service-requirement service))
  
  ;; Start the service
  (let ((result ((service-proc) #t))) ; #t indicates start
    (set-service-running?! service #t)
    (set-service-value! service result)
    (format #t "Service ~a started~%" (service-name service))
    result))

(define (stop-service service)
  "Stop a service."
  (unless (service-running? service)
    (error (string-append "Service " (service-name service) " is not running")))
  
  ;; Stop the service
  (let ((result ((service-proc) #f))) ; #f indicates stop
    (set-service-running?! service #f)
    (set-service-value! service #f)
    (format #t "Service ~a stopped~%" (service-name service))
    result))

(define (restart-service service)
  "Restart a service."
  (when (service-running? service)
    (stop-service service))
  (start-service service))

(define (reload-service service)
  "Reload a service."
  (when (service-running? service)
    (let ((result ((service-proc) 'reload))) ; 'reload indicates reload
      (set-service-value! service result)
      (format #t "Service ~a reloaded~%" (service-name service))
      result)
    (error (string-append "Service " (service-name service) " is not running"))))

(define (status-service service-name)
  "Get status of a service."
  (let ((service (find (lambda (s) (string=? (service-name s) service-name)) 
                       service-registry)))
    (if service
        (let ((status (if (service-running? service) "running" "stopped")))
          (format #t "Service ~a: ~a~%" service-name status)
          status)
        (error (string-append "Service " service-name " not found")))))

(define (register-service service)
  "Register a service."
  (when (find (lambda (s) (service-eq? s service)) service-registry)
    (error (string-append "Service " (service-name service) " already exists")))
  (set! service-registry (cons service service-registry))
  (format #t "Service ~a registered~%" (service-name service))
  service)

(define (unregister-service service)
  "Unregister a service."
  (when (service-running? service)
    (stop-service service))
  (set! service-registry (remove (lambda (s) (service-eq? s service)) service-registry))
  (format #t "Service ~a unregistered~%" (service-name service)))

(define (list-services)
  "List all registered services."
  (format #t "Registered services:~%")
  (for-each (lambda (service)
              (format #t "  - ~a: ~a~%" 
                      (service-name service)
                      (if (service-running? service) "running" "stopped")))
            service-registry))

;; Shepherd daemon functionality
(define (run-shepherd)
  "Run the shepherd daemon."
  (format #t "Starting shepherd daemon...~%")
  (let ((running #t))
    (while running
      (display "shepherd> ")
      (let ((input (read-line)))
        (match (string-tokenize input)
          (("quit") (set! running #f))
          (("exit") (set! running #f))
          (("start" name) 
           (let ((service (find (lambda (s) (string=? (service-name s) name)) 
                                service-registry)))
             (if service
                 (start-service service)
                 (format #t "Service ~a not found~%" name))))
          (("stop" name)
           (let ((service (find (lambda (s) (string=? (service-name s) name)) 
                                service-registry)))
             (if service
                 (stop-service service)
                 (format #t "Service ~a not found~%" name))))
          (("restart" name)
           (let ((service (find (lambda (s) (string=? (service-name s) name)) 
                                service-registry)))
             (if service
                 (restart-service service)
                 (format #t "Service ~a not found~%" name))))
          (("status" name)
           (status-service name))
          (("reload" name)
           (let ((service (find (lambda (s) (string=? (service-name s) name)) 
                                service-registry)))
             (if service
                 (reload-service service)
                 (format #t "Service ~a not found~%" name))))
          (("list")
           (list-services))
          ((command . args)
           (format #t "Unknown command: ~a~%" command))
          (()
           (void))))))
  (format #t "Shepherd daemon stopped~%"))