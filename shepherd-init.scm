;; Shepherd System Initialization
;; Sets up the Shepherd system with common services and configuration

(use-modules (ice-9 match)
             (ice-9 format)
             (ice-9 rdelim)
             (ice-9 control)
             (srfi srfi-1)
             (srfi srfi-9))

(use-module (shepherd))

;; System configuration
(define (load-system-config)
  "Load system configuration for Shepherd."
  `((system-name . "GNU Shepherd System")
    (hostname . "localhost")
    (default-services .
     (("network"
       "Network services"
       network-service-proc
       #:requirement '())
      ("syslog"
       "System logging service"
       syslog-service-proc
       #:requirement '())
      ("cron"
       "Periodic task scheduler"
       cron-service-proc
       #:requirement '())
      ("ssh"
       "SSH daemon"
       ssh-service-proc
       #:requirement '())))))

;; Network service procedure
(define (network-service-proc)
  "Procedure for network service."
  (lambda (action)
    (match action
      (#t
       (format #t "Starting network services...~%")
       ;; Bring up loopback interface
       (system "ip link set lo up")
       ;; Configure other network interfaces if needed
       (system "ip addr add 127.0.0.1/8 dev lo")
       "Network services started")
      (#f
       (format #t "Stopping network services...~%")
       ;; Bring down all interfaces except loopback
       (system "ip link set down dev eth0")
       (system "ip link set down dev eth1")
       (system "ip link set down dev wlan0")
       "Network services stopped")
      ('reload
       (format #t "Reloading network configuration...~%")
       (system "ip link down eth0; ip link up eth0")
       "Network configuration reloaded"))))

;; System logging service procedure
(define (syslog-service-proc)
  "Procedure for system logging service."
  (lambda (action)
    (match action
      (#t
       (format #t "Starting syslog service...~%")
       ;; Start syslog daemon
       (system "syslogd -n &")
       "Syslog service started")
      (#f
       (format #t "Stopping syslog service...~%")
       ;; Stop syslog daemon
       (system "killall syslogd")
       "Syslog service stopped")
      ('reload
       (format #t "Reloading syslog configuration...~%")
       (system "killall -HUP syslogd")
       "Syslog configuration reloaded"))))

;; Cron service procedure
(define (cron-service-proc)
  "Procedure for cron service."
  (lambda (action)
    (match action
      (#t
       (format #t "Starting cron service...~%")
       ;; Start cron daemon
       (system "crond -n &")
       "Cron service started")
      (#f
       (format #t "Stopping cron service...~%")
       ;; Stop cron daemon
       (system "killall crond")
       "Cron service stopped")
      ('reload
       (format #t "Reloading cron configuration...~%")
       (system "killall -HUP crond")
       "Cron configuration reloaded"))))

;; SSH service procedure
(define (ssh-service-proc)
  "Procedure for SSH service."
  (lambda (action)
    (match action
      (#t
       (format #t "Starting SSH service...~%")
       ;; Start SSH daemon
       (system "sshd -D &")
       "SSH service started")
      (#f
       (format #t "Stopping SSH service...~%")
       ;; Stop SSH daemon
       (system "killall sshd")
       "SSH service stopped")
      ('reload
       (format #t "Reloading SSH configuration...~%")
       (system "killall -HUP sshd")
       "SSH configuration reloaded"))))

;; Initialize basic services
(define (init-basic-services)
  "Initialize and register basic system services."
  (let ((config (load-system-config)))
    (for-each (lambda (service-info)
                (match service-info
                  ((name doc proc #:requirement (requirement))
                   (let ((service (make-service name doc proc 
                                                 #:requirement requirement
                                                 #:auto-start? #t)))
                     (register-service service)
                     (format #t "Initialized service: ~a~%" name)))))
              (assoc-ref config 'default-services))))

;; Load user services
(define (load-user-services)
  "Load user-defined services from configuration files."
  (let ((user-config-files (list "/etc/shepherd/user-services.scm"
                                 "~/.shepherd/services.scm"
                                 "./user-services.scm")))
    (for-each (lambda (config-file)
                (when (file-exists? config-file)
                  (format #t "Loading services from ~a...~%" config-file)
                  (load config-file)))
              user-config-files)))

;; Initialize system logging
(define (init-system-logging)
  "Initialize system logging for Shepherd."
  (let ((log-file "/var/log/shepherd.log")
        (log-dir "/var/log"))
    (unless (file-exists? log-dir)
      (mkdir-p log-dir))
    (with-output-to-file log-file
      (lambda ()
        (format #t "Shepherd system initialized at ~a~%" 
                (get-current-time))))))

;; Get current time
(define (get-current-time)
  "Get current formatted time."
  (let ((port (open-pipe "date" "r")))
    (let ((time-str (get-line port)))
      (close-port port)
      time-str)))

;; System initialization
(define (initialize-shepherd-system)
  "Initialize the complete Shepherd system."
  (format #t "Initializing Shepherd system...~%")
  
  ;; Initialize basic services
  (init-basic-services)
  
  ;; Load user services
  (load-user-services)
  
  ;; Initialize system logging
  (init-system-logging)
  
  (format #t "Shepherd system initialized successfully~%"))
  
;; Check system status
(define (check-system-status)
  "Check the status of the Shepherd system."
  (format #t "Shepherd System Status~%")
  (format #t "====================~%")
  (format #t "System name: ~a~%" (assoc-ref (load-system-config) 'system-name))
  (format #t "Hostname: ~a~%" (assoc-ref (load-system-config) 'hostname))
  (format #t "Registered services: ~a~%" (length service-registry))
  (format #t "Running services: ~a~%" 
          (length (filter service-running? service-registry)))
  (format #t "System time: ~a~%" (get-current-time)))

;; Main initialization
(initialize-shepherd-system)
(check-system-status)