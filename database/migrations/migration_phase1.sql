-- Migration: Phase 1 infrastructure tables
-- Target: MySQL 8.0+
-- Purpose:
--   1. idempotency_records: prevent duplicate submissions and replayed write commands.
--   2. outbox_events: persist domain events in the same transaction as business data.

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 1;

CREATE TABLE IF NOT EXISTS idempotency_records (
    id BIGINT NOT NULL AUTO_INCREMENT COMMENT 'Internal numeric primary key; keeps InnoDB secondary indexes compact',
    request_id VARCHAR(64)
        CHARACTER SET ascii COLLATE ascii_bin
        NOT NULL COMMENT 'Global idempotency key; the same logical request must reuse the same value',
    request_hash CHAR(64)
        CHARACTER SET ascii COLLATE ascii_bin
        NULL COMMENT 'SHA-256 hash of the request payload; detects replay with a different payload',
    module_code VARCHAR(64)
        CHARACTER SET ascii COLLATE ascii_general_ci
        NOT NULL COMMENT 'Business module code, for example registration or prescription',
    action_code VARCHAR(64)
        CHARACTER SET ascii COLLATE ascii_general_ci
        NOT NULL COMMENT 'Business action code, for example create, review, or dispense',
    business_key VARCHAR(128)
        CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci
        NULL COMMENT 'Business identifier such as registration number, prescription number, or bill number',
    operator_user_id BIGINT NULL COMMENT 'Operator user id resolved from the server session, never trusted from client input',
    client_ip VARCHAR(45)
        CHARACTER SET ascii COLLATE ascii_general_ci
        NULL COMMENT 'Client IP address, compatible with IPv4 and IPv6',
    status TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=PROCESSING, 1=SUCCESS, 2=FAILED, 3=COMPENSATED, 4=EXPIRED',
    result_code VARCHAR(32)
        CHARACTER SET ascii COLLATE ascii_general_ci
        NULL COMMENT 'Business result code; duplicate requests can reuse the previous result',
    result_message VARCHAR(500)
        CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci
        NULL COMMENT 'Business result message',
    result_payload JSON NULL COMMENT 'Business result JSON for duplicate request replay',
    locked_until DATETIME(3) NULL COMMENT 'Processing lock expiry time; used to recover stuck PROCESSING requests',
    expire_at DATETIME(3) NULL COMMENT 'Record expiry time for scheduled cleanup',
    created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT 'Creation time',
    updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
        ON UPDATE CURRENT_TIMESTAMP(3) COMMENT 'Last update time',

    PRIMARY KEY (id),
    UNIQUE KEY uk_idempotency_request_id (request_id),
    KEY idx_idempotency_module_action_status_time (module_code, action_code, status, created_at),
    KEY idx_idempotency_business_key (business_key),
    KEY idx_idempotency_status_lock (status, locked_until),
    KEY idx_idempotency_expire_at (expire_at),
    KEY idx_idempotency_operator_time (operator_user_id, created_at)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci
  ROW_FORMAT=DYNAMIC
  COMMENT='Idempotency records for duplicate submission prevention, replay protection, and MQ consumer idempotency';

CREATE TABLE IF NOT EXISTS outbox_events (
    id BIGINT NOT NULL AUTO_INCREMENT COMMENT 'Outbox sequence id; events are published in insertion order',
    event_id VARCHAR(64)
        CHARACTER SET ascii COLLATE ascii_bin
        NOT NULL COMMENT 'Global event id, for example UUID or snowflake id',
    dedupe_key VARCHAR(128)
        CHARACTER SET ascii COLLATE ascii_bin
        NULL COMMENT 'Optional event dedupe key such as aggregate_type:aggregate_id:event_type:version',
    event_type VARCHAR(64)
        CHARACTER SET ascii COLLATE ascii_general_ci
        NOT NULL COMMENT 'Event type, for example PrescriptionCreated or PrescriptionReviewed',
    aggregate_type VARCHAR(64)
        CHARACTER SET ascii COLLATE ascii_general_ci
        NOT NULL COMMENT 'Aggregate type, for example Prescription, Registration, or Inventory',
    aggregate_id BIGINT NOT NULL COMMENT 'Aggregate root id matching the business table primary key',
    business_key VARCHAR(128)
        CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci
        NULL COMMENT 'Business identifier such as RX prescription number or R registration number',
    route_key VARCHAR(128)
        CHARACTER SET ascii COLLATE ascii_general_ci
        NULL COMMENT 'MQ routing key or WebSocket channel, for example pharmacy.prescription.pending',
    payload JSON NOT NULL COMMENT 'Event payload JSON',
    headers JSON NULL COMMENT 'Event headers JSON, for example traceId, operatorUserId, and schemaVersion',
    status TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=NEW, 1=PUBLISHING, 2=PUBLISHED, 3=FAILED, 4=DEAD',
    retry_count SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Number of publish retries already attempted',
    max_retry SMALLINT UNSIGNED NOT NULL DEFAULT 10 COMMENT 'Maximum publish retry count before moving to DEAD',
    next_retry_at DATETIME(3) NULL COMMENT 'Next retry time, supports exponential backoff',
    locked_by VARCHAR(64)
        CHARACTER SET ascii COLLATE ascii_general_ci
        NULL COMMENT 'Publisher instance id currently locking this event',
    locked_at DATETIME(3) NULL COMMENT 'Time when the event was locked by a publisher',
    last_error VARCHAR(1000)
        CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci
        NULL COMMENT 'Last publish failure message',
    created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT 'Event creation time; committed with business data',
    updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
        ON UPDATE CURRENT_TIMESTAMP(3) COMMENT 'Last update time',
    published_at DATETIME(3) NULL COMMENT 'Successful publish time',

    PRIMARY KEY (id),
    UNIQUE KEY uk_outbox_event_id (event_id),
    UNIQUE KEY uk_outbox_dedupe_key (dedupe_key),
    KEY idx_outbox_poll (status, next_retry_at, id),
    KEY idx_outbox_lock_recovery (status, locked_at),
    KEY idx_outbox_aggregate_order (aggregate_type, aggregate_id, id),
    KEY idx_outbox_business_key (business_key),
    KEY idx_outbox_route_status (route_key, status, id),
    KEY idx_outbox_published_cleanup (status, published_at)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci
  ROW_FORMAT=DYNAMIC
  COMMENT='Transactional Outbox events for reliable MQ and WebSocket publication';
