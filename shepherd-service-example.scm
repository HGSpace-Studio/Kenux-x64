;; Example Shepherd Service Definitions
;; This file demonstrates how to define services for use with the shepherd system

(use-modules (shepherd))

;; A simple HTTP server service
(define (http-server-service-proc)
  "Procedure for HTTP server service."
  (lambda (action)
    (match action
      (#t
       (format #t "Starting HTTP server on port 8080~%")
       "HTTP server started")
      (#f
       (format #t "Stopping HTTP server~%")
       "HTTP server stopped")
      ('reload
       (format #t "Reloading HTTP server configuration~%")
       "HTTP server reloaded"))))

;; A database service
(define (database-service-proc)
  "Procedure for database service."
  (lambda (action)
    (match action
      (#t
       (format #t "Starting PostgreSQL database~%")
       (system "pg_ctl -D /var/lib/postgres start")
       "PostgreSQL started")
      (#f
       (format #t "Stopping PostgreSQL database~%")
       (system "pg_ctl -D /var/lib/postgres stop")
       "PostgreSQL stopped")
      ('reload
       (format #t "Reloading PostgreSQL configuration~%")
       (system "pg_ctl reload")
       "PostgreSQL reloaded"))))

;; A web application service
(define (web-app-service-proc)
  "Procedure for web application service."
  (lambda (action)
    (match action
      (#t
       (format #t "Starting web application~%")
       ;; Change this to your actual command
       (system "cd /path/to/app && npm start")
       "Web application started")
      (#f
       (format #t "Stopping web application~%")
       (system "pkill -f 'npm start'")
       "Web application stopped")
      ('reload
       (format #t "Restarting web application~%")
       (system "pkill -f 'npm start' && cd /path/to/app && npm start")
       "Web application restarted")))))

;; A logging service
(define (logging-service-proc)
  "Procedure for logging service."
  (lambda (action)
    (match action
      (#t
       (format #t "Starting log rotation service~%")
       (system "logrotate -f /etc/logrotate.conf")
       "Log rotation started")
      (#f
       (format #t "Stopping log rotation service~%")
       (system "pkill logrotate")
       "Log rotation stopped")
      ('reload
       (format #t "Reloading log configuration~%")
       (system "killall -HUP logrotate")
       "Log configuration reloaded")))))

;; Define the services
(define http-server-service
  (make-service "http-server"
                "HTTP server for serving web content"
                http-server-service-proc
                #:requirement '()
                #:auto-start? #t))

(define database-service
  (make-service "database"
                "PostgreSQL database service"
                database-service-proc
                #:requirement '()
                #:auto-start? #t))

(define web-app-service
  (make-service "web-app"
                "Main web application"
                web-app-service-proc
                #:requirement '("database")
                #:auto-start? #f))

(define logging-service
  (make-service "logging"
                "Log rotation service"
                logging-service-proc
                #:requirement '()
                #:auto-start? #t))

;; Register services
(register-service http-server-service)
(register-service database-service)
(register-service web-app-service)
(register-service logging-service)

;; Example of how to start services interactively
;; (start-service database-service)
;; (start-service http-server-service)
;; (start-service web-app-service)

;; Example of how to check service status
;; (status-service "http-server")

;; Example of how to stop services
;; (stop-service web-app-service)
;; (stop-service database-service)

(format #t "Example services defined. You can register them with:~%")
(format #t "  (register-service <service>)~%")
(format #t "And control them with:~%")
(format #t "  (start-service <service>)~%")
(format #t "  (stop-service <service>)~%")
(format #t "  (restart-service <service>)~%")
(format #t "  (status-service <service-name>)~%")