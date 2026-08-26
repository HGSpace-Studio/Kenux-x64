;; Shepherd Daemon - Process Management System
;; Main daemon implementation with enhanced functionality

(use-modules (ice-9 match)
             (ice-9 format)
             (ice-9 regex)
             (ice-9 rdelim)
             (ice-9 control)
             (srfi srfi-1)
             (srfi srfi-26)
             (srfi srfi-9)
             (srfi srfi-11))

(use-module (shepherd))

;; Enhanced service records with additional metadata
(define-record-type <enhanced-service>
  (make-enhanced-service base-service pid start-time log-file)
  enhanced-service?
  (base-service enhanced-service-base)
  (pid enhanced-service-pid set-enhanced-service-pid!)
  (start-time enhanced-service-start-time set-enhanced-service-start-time!)
  (log-file enhanced-service-log-file set-enhanced-service-log-file!))

;; Enhanced service registry
(define enhanced-service-registry '())

;; Service statistics
(define-record-type <service-stats>
  (make-service-stats starts stops restarts uptime last-error)
  service-stats?
  (starts service-stats-starts)
  (stops service-stats-stops)
  (restarts service-stats-restarts)
  (uptime service-stats-uptime)
  (last-error service-stats-last-error))

;; Default configuration
(define shepherd-config
  `((daemon-name . "shepherd")
    (pid-file . "/var/run/shepherd.pid")
    (log-file . "/var/log/shepherd.log")
    (max-restarts . 3)
    (restart-delay . 5)
    (check-interval . 30)
    (auto-restart? . #t)
    (log-level . 'info)))

;; Initialize daemon
(define (init-daemon)
  "Initialize the shepherd daemon."
  (format #t "Initializing Shepherd daemon...~%")
  
  ;; Create log directory if needed
  (let ((log-dir (dirname (assoc-ref shepherd-config 'log-file))))
    (unless (file-exists? log-dir)
      (mkdir-p log-dir)))
  
  ;; Write PID file
  (with-output-to-file (assoc-ref shepherd-config 'pid-file)
    (lambda ()
      (display (getpid))))
  
  (format #t "Daemon initialized~%"))

;; Cleanup daemon
(define (cleanup-daemon)
  "Clean up daemon resources."
  (format #t "Cleaning up daemon...~%")
  
  ;; Remove PID file
  (let ((pid-file (assoc-ref shepherd-config 'pid-file)))
    (when (file-exists? pid-file)
      (delete-file pid-file)))
  
  (format #t "Daemon cleaned up~%"))

;; Enhanced service management
(define (register-enhanced-service base-service)
  "Register an enhanced service."
  (let ((enhanced (make-enhanced-service base-service #f #f #f)))
    (set! enhanced-service-registry (cons enhanced enhanced-service-registry))
    enhanced))

(define (unregister-enhanced-service service-name)
  "Unregister an enhanced service."
  (let ((enhanced (find (lambda (s) 
                          (string=? (service-name (enhanced-service-base s)) service-name))
                        enhanced-service-registry)))
    (when enhanced
      (when (service-running? (enhanced-service-base enhanced))
        (stop-service (enhanced-service-base enhanced)))
      (set! enhanced-service-registry (remove (lambda (s) 
                                              (string=? (service-name (enhanced-service-base s)) service-name))
                                            enhanced-service-registry)))
  (format #t "Enhanced service ~a unregistered~%" service-name))

;; Enhanced service starting with monitoring
(define (start-service-with-monitoring service)
  "Start a service with monitoring enabled."
  (let ((enhanced (find (lambda (s) 
                          (service-eq? (enhanced-service-base s) service))
                        enhanced-service-registry)))
    (if (not enhanced)
        (register-enhanced-service service)
        enhanced))
  
  ;; Update statistics
  ;; For now, just start the service
  (start-service service))

;; Service monitoring
(define (monitor-services)
  "Monitor running services."
  (let ((interval (assoc-ref shepherd-config 'check-interval)))
    (let loop ()
      (sleep interval)
      (for-each (lambda (enhanced)
                  (let ((service (enhanced-service-base enhanced))
                        (pid (enhanced-service-pid enhanced)))
                    (when (and pid (service-running? service))
                      ;; Check if process is still running
                      (unless (zero? (system (string-append "kill -0 " (number->string pid))))
                        (format #t "Service ~a process died, restarting...~%" 
                                (service-name service))
                        ;; Mark as not running
                        (set-service-running?! service #f)
                        ;; Restart service
                        (when (assoc-ref shepherd-config 'auto-restart?)
                          (start-service-with-monitoring service))))))
                enhanced-service-registry))
      (loop))))

;; Enhanced daemon loop
(define (run-enhanced-daemon)
  "Run the enhanced shepherd daemon."
  (init-daemon)
  
  ;; Start monitoring thread
  (thread-start! (make-thread monitor-services))
  
  (let ((running #t))
    (while running
      (display "shepherd> ")
      (flush-output)
      (let ((input (read-line)))
        (match (string-tokenize input)
          (("quit")
           (set! running #f))
          (("exit")
           (set! running #f))
          (("start" name)
           (let ((service (find (lambda (s) (string=? (service-name s) name)) 
                                service-registry)))
             (if service
                 (start-service-with-monitoring service)
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
           (let ((enhanced (find (lambda (s) 
                                   (string=? (service-name (enhanced-service-base s)) name))
                                 enhanced-service-registry)))
             (if enhanced
                 (let ((service (enhanced-service-base enhanced))
                       (pid (enhanced-service-pid enhanced)))
                   (format #t "Service ~a: ~a~%" 
                           name
                           (if (service-running? service) "running" "stopped"))
                   (when pid
                     (format #t "  PID: ~a~%" pid))
                   (when (enhanced-service-start-time enhanced)
                     (format #t "  Started: ~a~%" 
                             (enhanced-service-start-time enhanced))))
                 (format #t "Service ~a not found~%" name))))
          (("list")
           (enhanced-list-services))
          (("stats" name)
           (let ((enhanced (find (lambda (s) 
                                   (string=? (service-name (enhanced-service-base s)) name))
                                 enhanced-service-registry)))
             (if enhanced
                 (enhanced-show-service-stats enhanced)
                 (format #t "Service ~a not found~%" name))))
          (("help")
           (show-daemon-help))
          (()
           (void))
          (_
           (format #t "Unknown command. Type 'help' for available commands~%"))))))
  
  (cleanup-daemon))

;; Enhanced listing
(define (enhanced-list-services)
  "List all enhanced services with detailed information."
  (format #t "Registered services:~%")
  (for-each (lambda (enhanced)
              (let ((service (enhanced-service-base enhanced))
                    (pid (enhanced-service-pid enhanced)))
                (format #t "  - ~a: ~a~%" 
                        (service-name service)
                        (if (service-running? service) "running" "stopped"))
                (when pid
                  (format #t "    PID: ~a~%" pid))
                (when (enhanced-service-start-time enhanced)
                  (format #t "    Started: ~a~%" 
                          (enhanced-service-start-time enhanced)))))
            enhanced-service-registry))

;; Show service statistics
(define (enhanced-show-service-stats enhanced)
  "Show statistics for an enhanced service."
  (let ((service (enhanced-service-base enhanced)))
    (format #t "Service ~a statistics:~%" (service-name service))
    (format #t "  Status: ~a~%" (if (service-running? service) "running" "stopped"))
    (when (enhanced-service-pid enhanced)
      (format #t "  PID: ~a~%" (enhanced-service-pid enhanced)))
    (when (enhanced-service-start-time enhanced)
      (format #t "  Started: ~a~%" (enhanced-service-start-time enhanced)))
    ;; Add more statistics as needed
    ))

;; Show daemon help
(define (show-daemon-help)
  "Show help for enhanced daemon commands."
  (display "Enhanced Shepherd Daemon Commands:\n")
  (display "  help                    Show this help message\n")
  (display "  start <service>         Start a service\n")
  (display "  stop <service>          Stop a service\n")
  (display "  restart <service>       Restart a service\n")
  (display "  status <service>        Show detailed service status\n")
  (display "  list                    List all services with details\n")
  (display "  stats <service>         Show service statistics\n")
  (display "  quit                    Exit the daemon\n")
  (display "  exit                    Exit the daemon\n"))

;; Main entry point for enhanced daemon
(define (main-daemon args)
  "Main entry point for enhanced daemon."
  (format #t "Starting Enhanced Shepherd Daemon...~%")
  (run-enhanced-daemon))