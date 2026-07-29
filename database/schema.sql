CREATE DATABASE IF NOT EXISTS hospital_outpatient
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

USE hospital_outpatient;

SET FOREIGN_KEY_CHECKS = 0;

DROP TABLE IF EXISTS audit_log_details;
DROP TABLE IF EXISTS operation_logs;
DROP TABLE IF EXISTS registration_insurance_audit_logs;
DROP TABLE IF EXISTS registration_insurance_tokens;
DROP TABLE IF EXISTS pass_rules;
DROP TABLE IF EXISTS fee_statistics_daily;
DROP TABLE IF EXISTS payments;
DROP TABLE IF EXISTS insurance_transactions;
DROP TABLE IF EXISTS bills;
DROP TABLE IF EXISTS stock_records;
DROP TABLE IF EXISTS prescription_items;
DROP TABLE IF EXISTS drugs;
DROP TABLE IF EXISTS drug_categories;
DROP TABLE IF EXISTS prescriptions;
DROP TABLE IF EXISTS examinations;
DROP TABLE IF EXISTS medical_records;
DROP TABLE IF EXISTS registrations;
DROP TABLE IF EXISTS registration_insurance_check;
DROP TABLE IF EXISTS schedule_rules;
DROP TABLE IF EXISTS doctor_schedules;
DROP TABLE IF EXISTS patients;
DROP TABLE IF EXISTS doctors;
DROP TABLE IF EXISTS departments;
DROP TABLE IF EXISTS users;
DROP TABLE IF EXISTS roles;
DROP TABLE IF EXISTS sys_user_dept;
DROP TABLE IF EXISTS sys_role_menu;
DROP TABLE IF EXISTS sys_user_role;
DROP TABLE IF EXISTS sys_menu;
DROP TABLE IF EXISTS sys_user;
DROP TABLE IF EXISTS sys_role;
DROP TABLE IF EXISTS sys_dept;

SET FOREIGN_KEY_CHECKS = 1;

CREATE TABLE roles (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    role_code VARCHAR(32) NOT NULL UNIQUE,
    role_name VARCHAR(64) NOT NULL,
    description VARCHAR(255),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    real_name VARCHAR(64) NOT NULL,
    phone VARCHAR(32),
    role_id BIGINT NOT NULL,
    status TINYINT NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (role_id) REFERENCES roles(id)
);

CREATE TABLE departments (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    dept_code VARCHAR(32) NOT NULL UNIQUE,
    dept_name VARCHAR(64) NOT NULL,
    location VARCHAR(128),
    status TINYINT NOT NULL DEFAULT 1
);

CREATE TABLE doctors (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL UNIQUE,
    department_id BIGINT NOT NULL,
    title VARCHAR(64),
    specialty VARCHAR(255),
    registration_fee DECIMAL(10,2) NOT NULL DEFAULT 0,
    status TINYINT NOT NULL DEFAULT 1,
    FOREIGN KEY (user_id) REFERENCES users(id),
    FOREIGN KEY (department_id) REFERENCES departments(id)
);

CREATE TABLE sys_dept (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    dept_code VARCHAR(32) NOT NULL UNIQUE,
    dept_name VARCHAR(64) NOT NULL,
    parent_id BIGINT,
    status TINYINT NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_sys_dept_parent (parent_id),
    FOREIGN KEY (parent_id) REFERENCES sys_dept(id)
);

CREATE TABLE sys_user (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    real_name VARCHAR(64) NOT NULL,
    phone VARCHAR(32),
    status TINYINT NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

CREATE TABLE sys_role (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    role_code VARCHAR(32) NOT NULL UNIQUE,
    role_name VARCHAR(64) NOT NULL,
    data_scope VARCHAR(32) NOT NULL DEFAULT 'SELF',
    description VARCHAR(255),
    status TINYINT NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

CREATE TABLE sys_menu (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    parent_id BIGINT,
    menu_code VARCHAR(64) NOT NULL UNIQUE,
    menu_name VARCHAR(64) NOT NULL,
    menu_type VARCHAR(16) NOT NULL,
    permission_code VARCHAR(128) UNIQUE,
    module_code VARCHAR(64),
    action_code VARCHAR(64),
    status TINYINT NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_sys_menu_parent (parent_id),
    FOREIGN KEY (parent_id) REFERENCES sys_menu(id)
);

CREATE TABLE sys_user_role (
    user_id BIGINT NOT NULL,
    role_id BIGINT NOT NULL,
    is_primary TINYINT NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, role_id),
    FOREIGN KEY (user_id) REFERENCES sys_user(id),
    FOREIGN KEY (role_id) REFERENCES sys_role(id)
);

CREATE TABLE sys_role_menu (
    role_id BIGINT NOT NULL,
    menu_id BIGINT NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (role_id, menu_id),
    FOREIGN KEY (role_id) REFERENCES sys_role(id),
    FOREIGN KEY (menu_id) REFERENCES sys_menu(id)
);

CREATE TABLE sys_user_dept (
    user_id BIGINT NOT NULL,
    dept_id BIGINT NOT NULL,
    is_primary TINYINT NOT NULL DEFAULT 0,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, dept_id),
    FOREIGN KEY (user_id) REFERENCES sys_user(id),
    FOREIGN KEY (dept_id) REFERENCES sys_dept(id)
);

CREATE TABLE patient_users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    password_salt VARCHAR(64) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

CREATE TABLE patients (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    patient_no VARCHAR(32) NOT NULL UNIQUE,
    user_id BIGINT NULL,
    username VARCHAR(64) NULL,
    password_hash VARCHAR(255) NULL,
    password_salt VARCHAR(64) NULL,
    name VARCHAR(64) NOT NULL,
    gender VARCHAR(8) NOT NULL,
    birthday DATE,
    id_card VARCHAR(32),
    phone VARCHAR(32),
    relationship VARCHAR(32) DEFAULT '本人',
    address VARCHAR(255),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_patients_username (username),
    UNIQUE KEY uk_patients_user_id_card (user_id, id_card),
    KEY idx_patients_user (user_id),
    CONSTRAINT fk_patients_user FOREIGN KEY (user_id) REFERENCES patient_users(id)
);

CREATE TABLE registration_insurance_check (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    patient_id BIGINT NOT NULL,
    id_card VARCHAR(32) NOT NULL,
    hospital_area_code VARCHAR(16) NOT NULL DEFAULT '110100',
    insured_area_code VARCHAR(16) NOT NULL DEFAULT '110100',
    insu_status VARCHAR(32) NOT NULL,
    valid_start_date DATE NOT NULL,
    valid_end_date DATE NOT NULL,
    is_remote_filed TINYINT NOT NULL DEFAULT 1,
    arrears_months INT NOT NULL DEFAULT 0,
    benefit_suspended TINYINT NOT NULL DEFAULT 0,
    insurance_type VARCHAR(32) NOT NULL,
    outpatient_pooling_supported TINYINT NOT NULL DEFAULT 1,
    annual_quota_total DECIMAL(10,2) NOT NULL DEFAULT 0,
    annual_quota_used DECIMAL(10,2) NOT NULL DEFAULT 0,
    quota_year INT NOT NULL,
    data_version BIGINT NOT NULL DEFAULT 1,
    check_enabled TINYINT NOT NULL DEFAULT 1,
    expected_result_code VARCHAR(16) NOT NULL,
    remark VARCHAR(255),
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_registration_insurance_patient (patient_id),
    KEY idx_registration_insurance_id_card (id_card),
    CONSTRAINT fk_registration_insurance_patient FOREIGN KEY (patient_id) REFERENCES patients(id)
);

CREATE TABLE registration_insurance_tokens (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    token_hash CHAR(64) NOT NULL UNIQUE,
    patient_id BIGINT NOT NULL,
    id_card VARCHAR(32) NOT NULL,
    hospital_area_code VARCHAR(16) NOT NULL,
    register_date DATE NOT NULL,
    department VARCHAR(64) NOT NULL,
    doctor VARCHAR(64) NOT NULL,
    time_slot VARCHAR(32) NOT NULL,
    data_version BIGINT NOT NULL,
    result_code VARCHAR(16) NOT NULL,
    expires_at DATETIME NOT NULL,
    used_at DATETIME NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_registration_insurance_token_patient (patient_id, expires_at),
    CONSTRAINT fk_registration_insurance_token_patient FOREIGN KEY (patient_id) REFERENCES patients(id)
);

CREATE TABLE registration_insurance_audit_logs (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    stage VARCHAR(32) NOT NULL,
    patient_id BIGINT NULL,
    operator_id BIGINT NULL,
    request_params JSON NOT NULL,
    result_code VARCHAR(16) NOT NULL,
    data_version BIGINT NULL,
    log_description VARCHAR(500) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_registration_insurance_audit_patient (patient_id, created_at),
    CONSTRAINT fk_registration_insurance_audit_patient FOREIGN KEY (patient_id) REFERENCES patients(id),
    CONSTRAINT fk_registration_insurance_audit_operator FOREIGN KEY (operator_id) REFERENCES users(id)
);

CREATE TABLE doctor_schedules (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    doctor_id BIGINT NOT NULL,
    department_id BIGINT,
    work_date DATE NOT NULL,
    period VARCHAR(16) NOT NULL,
    total_quota INT NOT NULL,
    remain_quota INT NOT NULL,
    status TINYINT NOT NULL DEFAULT 1,
    UNIQUE KEY uk_schedule (doctor_id, work_date, period),
    FOREIGN KEY (doctor_id) REFERENCES doctors(id),
    FOREIGN KEY (department_id) REFERENCES departments(id)
);

CREATE TABLE schedule_rules (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    rule_code VARCHAR(32) NOT NULL UNIQUE,
    rule_type VARCHAR(16) NOT NULL,
    target_type VARCHAR(16) NOT NULL,
    doctor_id BIGINT NULL,
    department_id BIGINT NULL,
    title VARCHAR(64) NULL,
    target_text VARCHAR(128) NULL,
    date_mode VARCHAR(16) NOT NULL,
    weekdays_mask INT NOT NULL DEFAULT 0,
    start_date DATE NULL,
    end_date DATE NULL,
    reason VARCHAR(255),
    raw_text VARCHAR(500),
    enabled TINYINT NOT NULL DEFAULT 1,
    created_by BIGINT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_schedule_rules_enabled (enabled),
    INDEX idx_schedule_rules_doctor (doctor_id, enabled),
    INDEX idx_schedule_rules_department (department_id, enabled),
    INDEX idx_schedule_rules_date (date_mode, start_date, end_date),
    FOREIGN KEY (doctor_id) REFERENCES doctors(id),
    FOREIGN KEY (department_id) REFERENCES departments(id),
    FOREIGN KEY (created_by) REFERENCES users(id)
);

CREATE TABLE registrations (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    registration_no VARCHAR(32) NOT NULL UNIQUE,
    user_id BIGINT NULL,
    patient_id BIGINT NOT NULL,
    doctor_id BIGINT NOT NULL,
    schedule_id BIGINT NOT NULL,
    appointment_time_slot VARCHAR(32),
    register_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    status VARCHAR(16) NOT NULL DEFAULT 'WAITING',
    fee DECIMAL(10,2) NOT NULL DEFAULT 0,
    insurance_result_code VARCHAR(16),
    insurance_token_no VARCHAR(64),
    payment_identity VARCHAR(32) NOT NULL DEFAULT 'SELF_PAY',
    is_emergency TINYINT NOT NULL DEFAULT 0,
    emergency_reason VARCHAR(255),
    operator_id BIGINT NOT NULL,
    FOREIGN KEY (user_id) REFERENCES patient_users(id),
    FOREIGN KEY (patient_id) REFERENCES patients(id),
    FOREIGN KEY (doctor_id) REFERENCES doctors(id),
    FOREIGN KEY (schedule_id) REFERENCES doctor_schedules(id),
    FOREIGN KEY (operator_id) REFERENCES users(id)
);

CREATE TABLE medical_records (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    registration_id BIGINT NOT NULL UNIQUE,
    chief_complaint VARCHAR(500),
    present_illness VARCHAR(1000),
    past_history VARCHAR(1000),
    physical_sign VARCHAR(1000),
    icd_code VARCHAR(32),
    diagnosis VARCHAR(500),
    advice VARCHAR(500),
    external_report_hospital VARCHAR(128),
    external_report_type VARCHAR(64),
    external_report_date DATE,
    external_report_summary VARCHAR(1000),
    external_report_conclusion VARCHAR(1000),
    external_report_attachment VARCHAR(500),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    doctor_id BIGINT NOT NULL,
    FOREIGN KEY (registration_id) REFERENCES registrations(id),
    FOREIGN KEY (doctor_id) REFERENCES doctors(id)
);

CREATE TABLE examinations (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    examination_no VARCHAR(32) NOT NULL UNIQUE,
    registration_id BIGINT NOT NULL,
    doctor_id BIGINT NOT NULL,
    item_id BIGINT NULL,
    item_name VARCHAR(128) NOT NULL,
    unit_price DECIMAL(10,2) NOT NULL DEFAULT 0,
    request_note VARCHAR(500),
    result_text VARCHAR(1000),
    report_finding VARCHAR(1000),
    report_conclusion VARCHAR(1000),
    report_attachment VARCHAR(500),
    status VARCHAR(16) NOT NULL DEFAULT 'PENDING',
    request_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    complete_time DATETIME NULL,
    FOREIGN KEY (registration_id) REFERENCES registrations(id),
    FOREIGN KEY (doctor_id) REFERENCES doctors(id)
);

CREATE TABLE examination_items (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    item_code VARCHAR(32) NOT NULL UNIQUE,
    item_name VARCHAR(128) NOT NULL UNIQUE,
    category VARCHAR(64) NOT NULL DEFAULT '检查',
    unit_price DECIMAL(10,2) NOT NULL DEFAULT 0,
    status TINYINT NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

CREATE TABLE prescriptions (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    prescription_no VARCHAR(32) NOT NULL UNIQUE,
    registration_id BIGINT NOT NULL,
    doctor_id BIGINT NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'CREATED',
    total_amount DECIMAL(10,2) NOT NULL DEFAULT 0,
    reviewer_id BIGINT NULL,
    review_time DATETIME NULL,
    reject_reason VARCHAR(500),
    dispense_user_id BIGINT NULL,
    dispense_time DATETIME NULL,
    return_user_id BIGINT NULL,
    return_time DATETIME NULL,
    return_reason VARCHAR(500),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (registration_id) REFERENCES registrations(id),
    FOREIGN KEY (doctor_id) REFERENCES doctors(id),
    FOREIGN KEY (reviewer_id) REFERENCES users(id),
    FOREIGN KEY (dispense_user_id) REFERENCES users(id),
    FOREIGN KEY (return_user_id) REFERENCES users(id)
);

CREATE TABLE drug_categories (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    category_name VARCHAR(64) NOT NULL UNIQUE,
    description VARCHAR(255)
);

CREATE TABLE drugs (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    drug_code VARCHAR(32) NOT NULL UNIQUE,
    barcode VARCHAR(64) UNIQUE,
    drug_name VARCHAR(128) NOT NULL,
    category_id BIGINT NOT NULL,
    specification VARCHAR(128),
    unit VARCHAR(16) NOT NULL,
    purchase_price DECIMAL(10,2) NOT NULL DEFAULT 0,
    sale_price DECIMAL(10,2) NOT NULL DEFAULT 0,
    stock_quantity INT NOT NULL DEFAULT 0,
    warning_quantity INT NOT NULL DEFAULT 10,
    expiry_date DATE NULL,
    is_special_control TINYINT NOT NULL DEFAULT 0,
    status TINYINT NOT NULL DEFAULT 1,
    FOREIGN KEY (category_id) REFERENCES drug_categories(id)
);

CREATE TABLE pass_rules (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    rule_code VARCHAR(32) NOT NULL UNIQUE,
    rule_type VARCHAR(32) NOT NULL,
    drug_name VARCHAR(128) NOT NULL,
    related_drug_name VARCHAR(128),
    patient_condition VARCHAR(128),
    warning_level VARCHAR(16) NOT NULL DEFAULT 'WARN',
    message VARCHAR(500) NOT NULL,
    enabled TINYINT NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE prescription_items (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    prescription_id BIGINT NOT NULL,
    drug_id BIGINT NOT NULL,
    quantity INT NOT NULL,
    dosage VARCHAR(128),
    frequency VARCHAR(128),
    days INT,
    unit_price DECIMAL(10,2) NOT NULL,
    amount DECIMAL(10,2) NOT NULL,
    FOREIGN KEY (prescription_id) REFERENCES prescriptions(id),
    FOREIGN KEY (drug_id) REFERENCES drugs(id)
);

CREATE TABLE stock_records (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    drug_id BIGINT NOT NULL,
    change_type VARCHAR(16) NOT NULL,
    quantity INT NOT NULL,
    before_quantity INT NOT NULL,
    after_quantity INT NOT NULL,
    related_no VARCHAR(64),
    operator_id BIGINT NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (drug_id) REFERENCES drugs(id),
    FOREIGN KEY (operator_id) REFERENCES users(id)
);

CREATE TABLE bills (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    bill_no VARCHAR(32) NOT NULL UNIQUE,
    user_id BIGINT NULL,
    registration_id BIGINT NOT NULL,
    patient_id BIGINT NOT NULL,
    registration_fee DECIMAL(10,2) NOT NULL DEFAULT 0,
    drug_fee DECIMAL(10,2) NOT NULL DEFAULT 0,
    other_fee DECIMAL(10,2) NOT NULL DEFAULT 0,
    total_amount DECIMAL(10,2) NOT NULL DEFAULT 0,
    status VARCHAR(32) NOT NULL DEFAULT 'UNPAID',
    pay_time DATETIME NULL,
    payment_token_hash CHAR(64) NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES patient_users(id),
    FOREIGN KEY (registration_id) REFERENCES registrations(id),
    FOREIGN KEY (patient_id) REFERENCES patients(id)
);

CREATE TABLE insurance_transactions (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    transaction_id VARCHAR(64) NOT NULL UNIQUE,
    bill_id BIGINT NOT NULL,
    patient_id BIGINT NOT NULL,
    amount DECIMAL(10,2) NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'PROCESSING',
    request_payload JSON NOT NULL,
    last_error VARCHAR(500),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (bill_id) REFERENCES bills(id),
    FOREIGN KEY (patient_id) REFERENCES patients(id)
);

CREATE TABLE payments (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    payment_no VARCHAR(32) NOT NULL UNIQUE,
    bill_id BIGINT NOT NULL,
    amount DECIMAL(10,2) NOT NULL,
    pay_method VARCHAR(32) NOT NULL,
    pay_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    cashier_id BIGINT NOT NULL,
    UNIQUE KEY uk_payments_bill (bill_id),
    FOREIGN KEY (bill_id) REFERENCES bills(id),
    FOREIGN KEY (cashier_id) REFERENCES users(id)
);

CREATE TABLE fee_statistics_daily (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    stat_date DATE NOT NULL,
    department_id BIGINT,
    registration_income DECIMAL(12,2) NOT NULL DEFAULT 0,
    drug_income DECIMAL(12,2) NOT NULL DEFAULT 0,
    total_income DECIMAL(12,2) NOT NULL DEFAULT 0,
    UNIQUE KEY uk_daily_department (stat_date, department_id),
    FOREIGN KEY (department_id) REFERENCES departments(id)
);

CREATE TABLE operation_logs (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT,
    module VARCHAR(64) NOT NULL,
    action VARCHAR(64) NOT NULL,
    content VARCHAR(500),
    ip_address VARCHAR(64),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id)
);

CREATE TABLE audit_log_details (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    operation_log_id BIGINT,
    user_id BIGINT,
    module VARCHAR(64) NOT NULL,
    action VARCHAR(64) NOT NULL,
    business_key VARCHAR(128),
    field_name VARCHAR(64) NOT NULL,
    old_value VARCHAR(1000),
    new_value VARCHAR(1000),
    change_reason VARCHAR(255),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (operation_log_id) REFERENCES operation_logs(id),
    FOREIGN KEY (user_id) REFERENCES users(id)
);

CREATE TABLE IF NOT EXISTS outbox_events (
    id BIGINT NOT NULL AUTO_INCREMENT,
    event_id VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    dedupe_key VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NULL,
    event_type VARCHAR(64) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,
    aggregate_type VARCHAR(64) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,
    aggregate_id BIGINT NOT NULL,
    business_key VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
    route_key VARCHAR(128) CHARACTER SET ascii COLLATE ascii_general_ci NULL,
    payload JSON NOT NULL,
    headers JSON NULL,
    status TINYINT UNSIGNED NOT NULL DEFAULT 0,
    retry_count SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    max_retry SMALLINT UNSIGNED NOT NULL DEFAULT 10,
    next_retry_at DATETIME(3) NULL,
    locked_by VARCHAR(64) CHARACTER SET ascii COLLATE ascii_general_ci NULL,
    locked_at DATETIME(3) NULL,
    last_error VARCHAR(1000) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
    created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
    published_at DATETIME(3) NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uk_outbox_event_id (event_id),
    UNIQUE KEY uk_outbox_dedupe_key (dedupe_key),
    KEY idx_outbox_poll (status, next_retry_at, id),
    KEY idx_outbox_lock_recovery (status, locked_at),
    KEY idx_outbox_aggregate_order (aggregate_type, aggregate_id, id),
    KEY idx_outbox_business_key (business_key),
    KEY idx_outbox_route_status (route_key, status, id),
    KEY idx_outbox_published_cleanup (status, published_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;

CREATE INDEX idx_registrations_doctor_status_time ON registrations (doctor_id, status, register_time);
CREATE INDEX idx_registrations_patient_time ON registrations (patient_id, register_time);
CREATE INDEX idx_bills_status_time ON bills (status, created_at);
CREATE INDEX idx_prescriptions_status_time ON prescriptions (status, created_at);
CREATE INDEX idx_operation_logs_time ON operation_logs (created_at);
CREATE INDEX idx_audit_log_details_key ON audit_log_details (business_key, created_at);

INSERT INTO roles (role_code, role_name, description) VALUES
('ADMIN', '系统管理员', '维护基础数据和账号'),
('DIRECTOR', '科主任', '查看本科室经营和诊疗数据'),
('REGISTRAR', '挂号员', '患者建档和门诊挂号'),
('DOCTOR', '医生', '接诊、诊断和开处方'),
('PHARMACIST', '药房人员', '发药和库存管理'),
('CASHIER', '收费员', '收费结算和退费');

INSERT INTO users (username, password_hash, real_name, phone, role_id) VALUES
('admin', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '系统管理员', '13800000001', 1),
('director01', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '内科门诊主任', '13800000007', 2),
('reg01', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '挂号员一号', '13800000002', 3),
('doctor01', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '张明', '13800000003', 4),
('doctor02', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '李华', '13800000004', 4),
('pharmacy01', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '药房管理员', '13800000005', 5),
('cashier01', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '收费员一号', '13800000006', 6),
('doctor03', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '周宁', '13800000008', 4),
('doctor04', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '陈晓', '13800000009', 4),
('doctor05', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '孙洁', '13800000010', 4),
('doctor06', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '刘洋', '13800000011', 4);

INSERT INTO departments (dept_code, dept_name, location) VALUES
('DEP001', '内科门诊', '门诊楼二层'),
('DEP00101', '心血管内科', '门诊楼二层A区'),
('DEP0010101', '心血管内科诊室', '门诊楼二层A区1诊室'),
('DEP0010102', '高血压门诊', '门诊楼二层A区2诊室'),
('DEP0010103', '冠心病门诊', '门诊楼二层A区3诊室'),
('DEP00102', '血液内科', '门诊楼二层B区'),
('DEP0010201', '血液内科诊室', '门诊楼二层B区1诊室'),
('DEP0010202', '儿童血液诊室', '门诊楼二层B区2诊室'),
('DEP0010203', '贫血门诊', '门诊楼二层B区3诊室'),
('DEP0010204', '骨髓瘤门诊', '门诊楼二层B区4诊室'),
('DEP00103', '肾内科', '门诊楼二层C区'),
('DEP0010301', '肾内科诊室', '门诊楼二层C区1诊室'),
('DEP0010302', '慢性肾病门诊', '门诊楼二层C区2诊室'),
('DEP0010303', '血液透析门诊', '门诊楼二层C区3诊室'),
('DEP00104', '呼吸内科', '门诊楼二层D区'),
('DEP0010401', '呼吸内科诊室', '门诊楼二层D区1诊室'),
('DEP0010402', '哮喘门诊', '门诊楼二层D区2诊室'),
('DEP0010403', '肺部感染门诊', '门诊楼二层D区3诊室'),
('DEP00105', '消化内科', '门诊楼二层E区'),
('DEP0010501', '消化内科诊室', '门诊楼二层E区1诊室'),
('DEP0010502', '胃肠门诊', '门诊楼二层E区2诊室'),
('DEP0010503', '肝病门诊', '门诊楼二层E区3诊室'),
('DEP00106', '内分泌科', '门诊楼二层F区'),
('DEP0010601', '内分泌科诊室', '门诊楼二层F区1诊室'),
('DEP0010602', '糖尿病门诊', '门诊楼二层F区2诊室'),
('DEP0010603', '甲状腺门诊', '门诊楼二层F区3诊室'),
('DEP002', '外科门诊', '门诊楼三层'),
('DEP00201', '普外科', '门诊楼三层A区'),
('DEP0020101', '普外科诊室', '门诊楼三层A区1诊室'),
('DEP0020102', '胃肠外科门诊', '门诊楼三层A区2诊室'),
('DEP0020103', '肝胆外科门诊', '门诊楼三层A区3诊室'),
('DEP00202', '骨科', '门诊楼三层B区'),
('DEP0020201', '骨科诊室', '门诊楼三层B区1诊室'),
('DEP0020202', '关节门诊', '门诊楼三层B区2诊室'),
('DEP0020203', '脊柱门诊', '门诊楼三层B区3诊室'),
('DEP003', '儿科门诊', '门诊楼一层'),
('DEP00301', '儿科普通', '门诊楼一层A区'),
('DEP0030101', '儿科普通诊室', '门诊楼一层A区1诊室'),
('DEP0030102', '儿童发热门诊', '门诊楼一层A区2诊室'),
('DEP00302', '儿童血液', '门诊楼一层B区'),
('DEP0030201', '儿童血液专病门诊', '门诊楼一层B区1诊室'),
('DEP0030202', '儿童贫血门诊', '门诊楼一层B区2诊室'),
('DEP004', '中医科门诊', '门诊楼四层'),
('DEP00401', '中医内科', '门诊楼四层A区'),
('DEP0040101', '中医内科诊室', '门诊楼四层A区1诊室'),
('DEP0040102', '失眠调理门诊', '门诊楼四层A区2诊室'),
('DEP0040103', '针灸门诊', '门诊楼四层A区3诊室');

INSERT INTO doctors (user_id, department_id, title, specialty, registration_fee) VALUES
(4, 2, '主任医师', '高血压、冠心病、心律失常、慢性病管理', 23.00),
(5, 28, '副主任医师', '普外科、创伤处理、腹痛待查', 18.00),
(8, 6, '主任医师', '贫血、白细胞异常、血小板减少、淋巴结肿大', 23.00),
(9, 15, '副主任医师', '咳嗽、哮喘、慢阻肺、肺部感染', 20.00),
(10, 40, '主任医师', '儿童发热、贫血、血液系统疾病', 22.00),
(11, 11, '副主任医师', '慢性肾病、蛋白尿、血液透析随访', 20.00);

-- 补齐门诊目录：每个大类、专科、诊室都可在医生管理和排班中选择。
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CAT001', '内科门诊', '门诊楼'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '内科门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC002', '心血管内科', '内科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '心血管内科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN003', '心血管内科诊室', '内科门诊-心血管内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '心血管内科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN004', '高血压门诊', '内科门诊-心血管内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '高血压门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN005', '冠心病门诊', '内科门诊-心血管内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '冠心病门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC006', '血液内科', '内科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '血液内科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN007', '血液内科诊室', '内科门诊-血液内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '血液内科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN008', '儿童血液诊室', '内科门诊-血液内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿童血液诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN009', '贫血门诊', '内科门诊-血液内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '贫血门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN010', '骨髓瘤门诊', '内科门诊-血液内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '骨髓瘤门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC011', '肾内科', '内科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '肾内科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN012', '肾内科诊室', '内科门诊-肾内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '肾内科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN013', '慢性肾病门诊', '内科门诊-肾内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '慢性肾病门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN014', '血液透析门诊', '内科门诊-肾内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '血液透析门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC015', '呼吸内科', '内科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '呼吸内科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN016', '呼吸内科诊室', '内科门诊-呼吸内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '呼吸内科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN017', '哮喘门诊', '内科门诊-呼吸内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '哮喘门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN018', '肺部感染门诊', '内科门诊-呼吸内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '肺部感染门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC019', '消化内科', '内科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '消化内科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN020', '消化内科诊室', '内科门诊-消化内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '消化内科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN021', '胃肠门诊', '内科门诊-消化内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '胃肠门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN022', '肝病门诊', '内科门诊-消化内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '肝病门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC023', '内分泌科', '内科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '内分泌科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN024', '内分泌科诊室', '内科门诊-内分泌科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '内分泌科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN025', '糖尿病门诊', '内科门诊-内分泌科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '糖尿病门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN026', '甲状腺门诊', '内科门诊-内分泌科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '甲状腺门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC027', '神经内科', '内科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '神经内科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN028', '神经内科诊室', '内科门诊-神经内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '神经内科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN029', '头痛门诊', '内科门诊-神经内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '头痛门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN030', '脑卒中随访门诊', '内科门诊-神经内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '脑卒中随访门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC031', '风湿免疫科', '内科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '风湿免疫科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN032', '风湿免疫科诊室', '内科门诊-风湿免疫科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '风湿免疫科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN033', '类风湿门诊', '内科门诊-风湿免疫科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '类风湿门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN034', '痛风门诊', '内科门诊-风湿免疫科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '痛风门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC035', '老年医学科', '内科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '老年医学科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN036', '老年医学科诊室', '内科门诊-老年医学科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '老年医学科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN037', '老年慢病门诊', '内科门诊-老年医学科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '老年慢病门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN038', '综合评估门诊', '内科门诊-老年医学科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '综合评估门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CAT039', '外科门诊', '门诊楼'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '外科门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC040', '普外科', '外科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '普外科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN041', '普外科诊室', '外科门诊-普外科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '普外科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN042', '胃肠外科门诊', '外科门诊-普外科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '胃肠外科门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN043', '肝胆外科门诊', '外科门诊-普外科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '肝胆外科门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC044', '骨科', '外科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '骨科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN045', '骨科诊室', '外科门诊-骨科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '骨科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN046', '关节门诊', '外科门诊-骨科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '关节门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN047', '脊柱门诊', '外科门诊-骨科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '脊柱门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC048', '泌尿外科', '外科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '泌尿外科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN049', '泌尿外科诊室', '外科门诊-泌尿外科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '泌尿外科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN050', '结石门诊', '外科门诊-泌尿外科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '结石门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN051', '前列腺门诊', '外科门诊-泌尿外科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '前列腺门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC052', '神经外科', '外科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '神经外科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN053', '神经外科诊室', '外科门诊-神经外科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '神经外科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN054', '颅脑外伤门诊', '外科门诊-神经外科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '颅脑外伤门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC055', '胸外科', '外科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '胸外科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN056', '胸外科诊室', '外科门诊-胸外科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '胸外科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN057', '肺结节门诊', '外科门诊-胸外科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '肺结节门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CAT058', '儿科门诊', '门诊楼'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿科门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC059', '儿科普通', '儿科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿科普通');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN060', '儿科普通诊室', '儿科门诊-儿科普通'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿科普通诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN061', '儿童发热门诊', '儿科门诊-儿科普通'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿童发热门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC062', '儿童血液', '儿科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿童血液');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN063', '儿童血液专病门诊', '儿科门诊-儿童血液'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿童血液专病门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN064', '儿童贫血门诊', '儿科门诊-儿童血液'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿童贫血门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC065', '儿童呼吸', '儿科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿童呼吸');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN066', '儿童呼吸诊室', '儿科门诊-儿童呼吸'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿童呼吸诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN067', '儿童哮喘门诊', '儿科门诊-儿童呼吸'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿童哮喘门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC068', '儿童消化', '儿科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿童消化');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN069', '儿童消化诊室', '儿科门诊-儿童消化'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿童消化诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN070', '儿童腹痛门诊', '儿科门诊-儿童消化'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '儿童腹痛门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CAT071', '妇产科门诊', '门诊楼'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '妇产科门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC072', '妇科', '妇产科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '妇科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN073', '妇科诊室', '妇产科门诊-妇科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '妇科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN074', '宫颈疾病门诊', '妇产科门诊-妇科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '宫颈疾病门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN075', '月经病门诊', '妇产科门诊-妇科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '月经病门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC076', '产科', '妇产科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '产科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN077', '产科诊室', '妇产科门诊-产科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '产科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN078', '孕期保健门诊', '妇产科门诊-产科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '孕期保健门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN079', '高危妊娠门诊', '妇产科门诊-产科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '高危妊娠门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CAT080', '眼耳鼻喉门诊', '门诊楼'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '眼耳鼻喉门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC081', '眼科', '眼耳鼻喉门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '眼科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN082', '眼科诊室', '眼耳鼻喉门诊-眼科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '眼科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN083', '视光门诊', '眼耳鼻喉门诊-眼科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '视光门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN084', '白内障门诊', '眼耳鼻喉门诊-眼科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '白内障门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC085', '耳鼻喉科', '眼耳鼻喉门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '耳鼻喉科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN086', '耳鼻喉科诊室', '眼耳鼻喉门诊-耳鼻喉科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '耳鼻喉科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN087', '鼻炎门诊', '眼耳鼻喉门诊-耳鼻喉科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '鼻炎门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN088', '咽喉门诊', '眼耳鼻喉门诊-耳鼻喉科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '咽喉门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CAT089', '口腔科门诊', '门诊楼'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '口腔科门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC090', '口腔内科', '口腔科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '口腔内科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN091', '口腔内科诊室', '口腔科门诊-口腔内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '口腔内科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN092', '牙体牙髓门诊', '口腔科门诊-口腔内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '牙体牙髓门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC093', '口腔修复科', '口腔科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '口腔修复科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN094', '口腔修复诊室', '口腔科门诊-口腔修复科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '口腔修复诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN095', '种植修复门诊', '口腔科门诊-口腔修复科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '种植修复门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CAT096', '皮肤科门诊', '门诊楼'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '皮肤科门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC097', '皮肤科', '皮肤科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '皮肤科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN098', '皮肤科诊室', '皮肤科门诊-皮肤科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '皮肤科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN099', '湿疹门诊', '皮肤科门诊-皮肤科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '湿疹门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN100', '痤疮门诊', '皮肤科门诊-皮肤科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '痤疮门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CAT101', '中医科门诊', '门诊楼'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '中医科门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC102', '中医内科', '中医科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '中医内科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN103', '中医内科诊室', '中医科门诊-中医内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '中医内科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN104', '失眠调理门诊', '中医科门诊-中医内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '失眠调理门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN105', '针灸门诊', '中医科门诊-中医内科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '针灸门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC106', '中医妇科', '中医科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '中医妇科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN107', '中医妇科诊室', '中医科门诊-中医妇科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '中医妇科诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN108', '月经调理门诊', '中医科门诊-中医妇科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '月经调理门诊');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'SPC109', '康复理疗科', '中医科门诊'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '康复理疗科');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN110', '康复理疗诊室', '中医科门诊-康复理疗科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '康复理疗诊室');
INSERT INTO departments (dept_code, dept_name, location)
SELECT 'CLN111', '颈肩腰腿痛门诊', '中医科门诊-康复理疗科'
WHERE NOT EXISTS (SELECT 1 FROM departments WHERE dept_name = '颈肩腰腿痛门诊');

-- 补齐目录医生：费用按项目演示口径映射公开医事服务费档位（副主任/主任/知名专家）。
INSERT IGNORE INTO users (username, password_hash, real_name, phone, role_id) VALUES
('catalog_doctor001', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '郑凯', '13920000001', 4),
('catalog_doctor002', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '王立群', '13920000002', 4),
('catalog_doctor003', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '冯若楠', '13920000003', 4),
('catalog_doctor004', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '韩亦辰', '13920000004', 4),
('catalog_doctor005', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '许清源', '13920000005', 4),
('catalog_doctor006', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '沈嘉禾', '13920000006', 4),
('catalog_doctor007', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '唐雨薇', '13920000007', 4),
('catalog_doctor008', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '曹明远', '13920000008', 4),
('catalog_doctor009', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '梁思远', '13920000009', 4),
('catalog_doctor010', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '杜若溪', '13920000010', 4),
('catalog_doctor011', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '程浩然', '13920000011', 4),
('catalog_doctor012', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '叶安琪', '13920000012', 4),
('catalog_doctor013', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '薛景行', '13920000013', 4),
('catalog_doctor014', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '顾晓曼', '13920000014', 4),
('catalog_doctor015', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '罗云舟', '13920000015', 4),
('catalog_doctor016', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '林知远', '13920000016', 4),
('catalog_doctor017', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '马思齐', '13920000017', 4),
('catalog_doctor018', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '高子昂', '13920000018', 4),
('catalog_doctor019', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '宋雅宁', '13920000019', 4),
('catalog_doctor020', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '何沐阳', '13920000020', 4),
('catalog_doctor021', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '魏舒然', '13920000021', 4),
('catalog_doctor022', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '潘嘉宁', '13920000022', 4),
('catalog_doctor023', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '方俊逸', '13920000023', 4),
('catalog_doctor024', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '袁静姝', '13920000024', 4),
('catalog_doctor025', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '邹亦凡', '13920000025', 4),
('catalog_doctor026', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '邵佳怡', '13920000026', 4),
('catalog_doctor027', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '钱思源', '13920000027', 4),
('catalog_doctor028', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '贺明轩', '13920000028', 4),
('catalog_doctor029', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '孟雨桐', '13920000029', 4),
('catalog_doctor030', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '戴清和', '13920000030', 4),
('catalog_doctor031', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '陆子涵', '13920000031', 4),
('catalog_doctor032', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '崔明朗', '13920000032', 4),
('catalog_doctor033', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '夏以宁', '13920000033', 4),
('catalog_doctor034', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '钟景澄', '13920000034', 4),
('catalog_doctor035', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '姜若谷', '13920000035', 4),
('catalog_doctor036', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '白承泽', '13920000036', 4),
('catalog_doctor037', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '田梓涵', '13920000037', 4),
('catalog_doctor038', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '孔令仪', '13920000038', 4),
('catalog_doctor039', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '严子墨', '13920000039', 4),
('catalog_doctor040', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '任清妍', '13920000040', 4),
('catalog_doctor041', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '施予安', '13920000041', 4),
('catalog_doctor042', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '石文博', '13920000042', 4),
('catalog_doctor043', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '范若晨', '13920000043', 4),
('catalog_doctor044', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '乔思淼', '13920000044', 4),
('catalog_doctor045', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '邱逸凡', '13920000045', 4),
('catalog_doctor046', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '洪嘉树', '13920000046', 4),
('catalog_doctor047', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '赖雨晴', '13920000047', 4),
('catalog_doctor048', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '秦明哲', '13920000048', 4),
('catalog_doctor049', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '倪语嫣', '13920000049', 4),
('catalog_doctor050', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '汤书航', '13920000050', 4),
('catalog_doctor051', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '尹静远', '13920000051', 4),
('catalog_doctor052', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '常安然', '13920000052', 4),
('catalog_doctor053', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '黎昊天', '13920000053', 4),
('catalog_doctor054', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '莫清秋', '13920000054', 4),
('catalog_doctor055', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '傅子瑜', '13920000055', 4),
('catalog_doctor056', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '万嘉懿', '13920000056', 4),
('catalog_doctor057', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '江明玥', '13920000057', 4),
('catalog_doctor058', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '段景然', '13920000058', 4),
('catalog_doctor059', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '梅若曦', '13920000059', 4),
('catalog_doctor060', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '卢泽宇', '13920000060', 4),
('catalog_doctor061', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '郝星河', '13920000061', 4),
('catalog_doctor062', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '毕语堂', '13920000062', 4),
('catalog_doctor063', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '康明睿', '13920000063', 4),
('catalog_doctor064', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '毛思远', '13920000064', 4),
('catalog_doctor065', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '文嘉禾', '13920000065', 4),
('catalog_doctor066', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '葛雨辰', '13920000066', 4),
('catalog_doctor067', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '詹知夏', '13920000067', 4),
('catalog_doctor068', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '樊若云', '13920000068', 4),
('catalog_doctor069', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '纪明扬', '13920000069', 4),
('catalog_doctor070', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '温清越', '13920000070', 4),
('catalog_doctor071', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '龙启航', '13920000071', 4),
('catalog_doctor072', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '向思衡', '13920000072', 4),
('catalog_doctor073', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '苗若琳', '13920000073', 4),
('catalog_doctor074', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '申明远', '13920000074', 4),
('catalog_doctor075', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '欧阳静安', '13920000075', 4);

INSERT INTO doctors (user_id, department_id, title, specialty, registration_fee, status)
SELECT u.id, d.id, seed.title, seed.specialty, seed.fee, 1
FROM (
SELECT 'catalog_doctor001' AS username, '心血管内科诊室' AS dept_name, '副主任医师' AS title, '心血管内科常见病、多发病及心血管内科诊室专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor002' AS username, '高血压门诊' AS dept_name, '主任医师' AS title, '心血管内科常见病、多发病及高血压门诊专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor003' AS username, '冠心病门诊' AS dept_name, '知名专家（三档）' AS title, '心血管内科常见病、多发病及冠心病门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor004' AS username, '血液内科诊室' AS dept_name, '知名专家（四档）' AS title, '血液内科常见病、多发病及血液内科诊室专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor005' AS username, '儿童血液诊室' AS dept_name, '副主任医师' AS title, '血液内科常见病、多发病及儿童血液诊室专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor006' AS username, '贫血门诊' AS dept_name, '主任医师' AS title, '血液内科常见病、多发病及贫血门诊专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor007' AS username, '骨髓瘤门诊' AS dept_name, '知名专家（三档）' AS title, '血液内科常见病、多发病及骨髓瘤门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor008' AS username, '肾内科诊室' AS dept_name, '知名专家（四档）' AS title, '肾内科常见病、多发病及肾内科诊室专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor009' AS username, '慢性肾病门诊' AS dept_name, '副主任医师' AS title, '肾内科常见病、多发病及慢性肾病门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor010' AS username, '血液透析门诊' AS dept_name, '主任医师' AS title, '肾内科常见病、多发病及血液透析门诊专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor011' AS username, '呼吸内科诊室' AS dept_name, '知名专家（三档）' AS title, '呼吸内科常见病、多发病及呼吸内科诊室专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor012' AS username, '哮喘门诊' AS dept_name, '知名专家（四档）' AS title, '呼吸内科常见病、多发病及哮喘门诊专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor013' AS username, '肺部感染门诊' AS dept_name, '副主任医师' AS title, '呼吸内科常见病、多发病及肺部感染门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor014' AS username, '消化内科诊室' AS dept_name, '主任医师' AS title, '消化内科常见病、多发病及消化内科诊室专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor015' AS username, '胃肠门诊' AS dept_name, '知名专家（三档）' AS title, '消化内科常见病、多发病及胃肠门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor016' AS username, '肝病门诊' AS dept_name, '知名专家（四档）' AS title, '消化内科常见病、多发病及肝病门诊专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor017' AS username, '内分泌科诊室' AS dept_name, '副主任医师' AS title, '内分泌科常见病、多发病及内分泌科诊室专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor018' AS username, '糖尿病门诊' AS dept_name, '主任医师' AS title, '内分泌科常见病、多发病及糖尿病门诊专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor019' AS username, '甲状腺门诊' AS dept_name, '知名专家（三档）' AS title, '内分泌科常见病、多发病及甲状腺门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor020' AS username, '神经内科诊室' AS dept_name, '知名专家（四档）' AS title, '神经内科常见病、多发病及神经内科诊室专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor021' AS username, '头痛门诊' AS dept_name, '副主任医师' AS title, '神经内科常见病、多发病及头痛门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor022' AS username, '脑卒中随访门诊' AS dept_name, '主任医师' AS title, '神经内科常见病、多发病及脑卒中随访门诊专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor023' AS username, '风湿免疫科诊室' AS dept_name, '知名专家（三档）' AS title, '风湿免疫科常见病、多发病及风湿免疫科诊室专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor024' AS username, '类风湿门诊' AS dept_name, '知名专家（四档）' AS title, '风湿免疫科常见病、多发病及类风湿门诊专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor025' AS username, '痛风门诊' AS dept_name, '副主任医师' AS title, '风湿免疫科常见病、多发病及痛风门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor026' AS username, '老年医学科诊室' AS dept_name, '主任医师' AS title, '老年医学科常见病、多发病及老年医学科诊室专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor027' AS username, '老年慢病门诊' AS dept_name, '知名专家（三档）' AS title, '老年医学科常见病、多发病及老年慢病门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor028' AS username, '综合评估门诊' AS dept_name, '知名专家（四档）' AS title, '老年医学科常见病、多发病及综合评估门诊专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor029' AS username, '普外科诊室' AS dept_name, '副主任医师' AS title, '普外科常见病、多发病及普外科诊室专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor030' AS username, '胃肠外科门诊' AS dept_name, '主任医师' AS title, '普外科常见病、多发病及胃肠外科门诊专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor031' AS username, '肝胆外科门诊' AS dept_name, '知名专家（三档）' AS title, '普外科常见病、多发病及肝胆外科门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor032' AS username, '骨科诊室' AS dept_name, '知名专家（四档）' AS title, '骨科常见病、多发病及骨科诊室专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor033' AS username, '关节门诊' AS dept_name, '副主任医师' AS title, '骨科常见病、多发病及关节门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor034' AS username, '脊柱门诊' AS dept_name, '主任医师' AS title, '骨科常见病、多发病及脊柱门诊专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor035' AS username, '泌尿外科诊室' AS dept_name, '知名专家（三档）' AS title, '泌尿外科常见病、多发病及泌尿外科诊室专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor036' AS username, '结石门诊' AS dept_name, '知名专家（四档）' AS title, '泌尿外科常见病、多发病及结石门诊专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor037' AS username, '前列腺门诊' AS dept_name, '副主任医师' AS title, '泌尿外科常见病、多发病及前列腺门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor038' AS username, '神经外科诊室' AS dept_name, '主任医师' AS title, '神经外科常见病、多发病及神经外科诊室专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor039' AS username, '颅脑外伤门诊' AS dept_name, '知名专家（三档）' AS title, '神经外科常见病、多发病及颅脑外伤门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor040' AS username, '胸外科诊室' AS dept_name, '知名专家（四档）' AS title, '胸外科常见病、多发病及胸外科诊室专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor041' AS username, '肺结节门诊' AS dept_name, '副主任医师' AS title, '胸外科常见病、多发病及肺结节门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor042' AS username, '儿科普通诊室' AS dept_name, '主任医师' AS title, '儿科普通常见病、多发病及儿科普通诊室专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor043' AS username, '儿童发热门诊' AS dept_name, '知名专家（三档）' AS title, '儿科普通常见病、多发病及儿童发热门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor044' AS username, '儿童血液专病门诊' AS dept_name, '知名专家（四档）' AS title, '儿童血液常见病、多发病及儿童血液专病门诊专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor045' AS username, '儿童贫血门诊' AS dept_name, '副主任医师' AS title, '儿童血液常见病、多发病及儿童贫血门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor046' AS username, '儿童呼吸诊室' AS dept_name, '主任医师' AS title, '儿童呼吸常见病、多发病及儿童呼吸诊室专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor047' AS username, '儿童哮喘门诊' AS dept_name, '知名专家（三档）' AS title, '儿童呼吸常见病、多发病及儿童哮喘门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor048' AS username, '儿童消化诊室' AS dept_name, '知名专家（四档）' AS title, '儿童消化常见病、多发病及儿童消化诊室专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor049' AS username, '儿童腹痛门诊' AS dept_name, '副主任医师' AS title, '儿童消化常见病、多发病及儿童腹痛门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor050' AS username, '妇科诊室' AS dept_name, '主任医师' AS title, '妇科常见病、多发病及妇科诊室专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor051' AS username, '宫颈疾病门诊' AS dept_name, '知名专家（三档）' AS title, '妇科常见病、多发病及宫颈疾病门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor052' AS username, '月经病门诊' AS dept_name, '知名专家（四档）' AS title, '妇科常见病、多发病及月经病门诊专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor053' AS username, '产科诊室' AS dept_name, '副主任医师' AS title, '产科常见病、多发病及产科诊室专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor054' AS username, '孕期保健门诊' AS dept_name, '主任医师' AS title, '产科常见病、多发病及孕期保健门诊专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor055' AS username, '高危妊娠门诊' AS dept_name, '知名专家（三档）' AS title, '产科常见病、多发病及高危妊娠门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor056' AS username, '眼科诊室' AS dept_name, '知名专家（四档）' AS title, '眼科常见病、多发病及眼科诊室专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor057' AS username, '视光门诊' AS dept_name, '副主任医师' AS title, '眼科常见病、多发病及视光门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor058' AS username, '白内障门诊' AS dept_name, '主任医师' AS title, '眼科常见病、多发病及白内障门诊专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor059' AS username, '耳鼻喉科诊室' AS dept_name, '知名专家（三档）' AS title, '耳鼻喉科常见病、多发病及耳鼻喉科诊室专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor060' AS username, '鼻炎门诊' AS dept_name, '知名专家（四档）' AS title, '耳鼻喉科常见病、多发病及鼻炎门诊专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor061' AS username, '咽喉门诊' AS dept_name, '副主任医师' AS title, '耳鼻喉科常见病、多发病及咽喉门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor062' AS username, '口腔内科诊室' AS dept_name, '主任医师' AS title, '口腔内科常见病、多发病及口腔内科诊室专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor063' AS username, '牙体牙髓门诊' AS dept_name, '知名专家（三档）' AS title, '口腔内科常见病、多发病及牙体牙髓门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor064' AS username, '口腔修复诊室' AS dept_name, '知名专家（四档）' AS title, '口腔修复科常见病、多发病及口腔修复诊室专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor065' AS username, '种植修复门诊' AS dept_name, '副主任医师' AS title, '口腔修复科常见病、多发病及种植修复门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor066' AS username, '皮肤科诊室' AS dept_name, '主任医师' AS title, '皮肤科常见病、多发病及皮肤科诊室专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor067' AS username, '湿疹门诊' AS dept_name, '知名专家（三档）' AS title, '皮肤科常见病、多发病及湿疹门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor068' AS username, '痤疮门诊' AS dept_name, '知名专家（四档）' AS title, '皮肤科常见病、多发病及痤疮门诊专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor069' AS username, '中医内科诊室' AS dept_name, '副主任医师' AS title, '中医内科常见病、多发病及中医内科诊室专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor070' AS username, '失眠调理门诊' AS dept_name, '主任医师' AS title, '中医内科常见病、多发病及失眠调理门诊专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor071' AS username, '针灸门诊' AS dept_name, '知名专家（三档）' AS title, '中医内科常见病、多发病及针灸门诊专病诊疗' AS specialty, 30.00 AS fee
UNION ALL SELECT 'catalog_doctor072' AS username, '中医妇科诊室' AS dept_name, '知名专家（四档）' AS title, '中医妇科常见病、多发病及中医妇科诊室专病诊疗' AS specialty, 50.00 AS fee
UNION ALL SELECT 'catalog_doctor073' AS username, '月经调理门诊' AS dept_name, '副主任医师' AS title, '中医妇科常见病、多发病及月经调理门诊专病诊疗' AS specialty, 20.00 AS fee
UNION ALL SELECT 'catalog_doctor074' AS username, '康复理疗诊室' AS dept_name, '主任医师' AS title, '康复理疗科常见病、多发病及康复理疗诊室专病诊疗' AS specialty, 25.00 AS fee
UNION ALL SELECT 'catalog_doctor075' AS username, '颈肩腰腿痛门诊' AS dept_name, '知名专家（三档）' AS title, '康复理疗科常见病、多发病及颈肩腰腿痛门诊专病诊疗' AS specialty, 30.00 AS fee
) seed
JOIN users u ON u.username = seed.username
JOIN departments d ON d.dept_name = seed.dept_name
LEFT JOIN doctors existing ON existing.user_id = u.id
WHERE existing.id IS NULL;


INSERT INTO patients (patient_no, name, gender, birthday, id_card, phone, address) VALUES
('P20260001', '王小兰', '女', '1996-03-18', '110101199603180021', '13910000001', '北京市海淀区知春路18号'),
('P20260002', '赵强', '男', '1988-11-02', '110101198811020033', '13910000002', '北京市朝阳区望京西园'),
('P20260003', '陈晨', '女', '2018-07-09', '110101201807090044', '13910000003', '北京市西城区'),
('P20260004', '李建国', '男', '1972-05-06', '110101197205060055', '13910000004', '北京市丰台区'),
('P20260005', '刘芳', '女', '1991-12-12', '110101199112120066', '13910000005', '北京市通州区'),
('P20260006', '孙浩', '男', '2001-02-03', '110101200102030077', NULL, NULL),
('P20260007', '周雨桐', '女', '1999-09-21', '110101199909210099', '13910000007', '北京市昌平区'),
('P20260008', '吴一鸣', '男', '2015-09-01', '110101201509010088', '13910000008', '北京市东城区');

INSERT INTO registration_insurance_check
(patient_id, id_card, hospital_area_code, insured_area_code, insu_status, valid_start_date, valid_end_date,
 is_remote_filed, arrears_months, benefit_suspended, insurance_type, outpatient_pooling_supported,
 annual_quota_total, annual_quota_used, quota_year, data_version, check_enabled, expected_result_code, remark) VALUES
(1, '110101199603180021', '110100', '110100', 'ACTIVE', DATE_SUB(CURRENT_DATE, INTERVAL 3 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR),
 1, 0, 0, 'URBAN_EMPLOYEE', 1, 2000.00, 300.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_000', '正常参保，预期允许医保统筹挂号'),
(2, '110101198811020033', '110100', '110100', 'EXPIRED', DATE_SUB(CURRENT_DATE, INTERVAL 3 YEAR), DATE_SUB(CURRENT_DATE, INTERVAL 1 DAY),
 1, 0, 0, 'URBAN_EMPLOYEE', 1, 2000.00, 200.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_002', '医保已过期，预期阻断医保统筹挂号'),
(3, '110101201807090044', '110100', '310100', 'ACTIVE', DATE_SUB(CURRENT_DATE, INTERVAL 2 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR),
 0, 0, 0, 'URBAN_RESIDENT', 1, 1200.00, 200.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_003', '异地未备案，预期阻断医保统筹挂号'),
(4, '110101197205060055', '110100', '110100', 'ACTIVE', DATE_SUB(CURRENT_DATE, INTERVAL 2 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR),
 1, 4, 1, 'URBAN_EMPLOYEE', 1, 2000.00, 100.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_004', '欠费停保，预期阻断医保统筹挂号'),
(5, '110101199112120066', '110100', '110100', 'ACTIVE', DATE_SUB(CURRENT_DATE, INTERVAL 2 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR),
 1, 0, 0, 'WORK_INJURY_ONLY', 0, 0.00, 0.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_005', '险种不支持门诊统筹，预期阻断医保统筹挂号'),
(6, '110101200102030077', '110100', '110100', 'ACTIVE', DATE_SUB(CURRENT_DATE, INTERVAL 2 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR),
 1, 0, 0, 'URBAN_EMPLOYEE', 1, 800.00, 800.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_006', '年度门诊统筹额度已用尽，预期阻断医保统筹挂号'),
(7, '110101199909210099', '110100', '110100', 'NO_INSURANCE', DATE_SUB(CURRENT_DATE, INTERVAL 2 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR),
 1, 0, 0, 'NONE', 0, 0.00, 0.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_001', '无医保参保信息，预期阻断医保统筹挂号');

INSERT INTO doctor_schedules (doctor_id, department_id, work_date, period, total_quota, remain_quota) VALUES
(1, 3, DATE_SUB(CURRENT_DATE, INTERVAL 1 DAY), '全天', 30, 29),
(1, 3, CURRENT_DATE, '全天', 30, 28),
(1, 3, DATE_ADD(CURRENT_DATE, INTERVAL 1 DAY), '全天', 30, 30),
(2, 29, CURRENT_DATE, '全天', 30, 29),
(2, 29, DATE_ADD(CURRENT_DATE, INTERVAL 1 DAY), '全天', 20, 20),
(3, 7, CURRENT_DATE, '全天', 25, 24),
(4, 16, CURRENT_DATE, '全天', 25, 24),
(5, 41, DATE_ADD(CURRENT_DATE, INTERVAL 1 DAY), '全天', 20, 19),
(6, 12, CURRENT_DATE, '全天', 25, 24),
(6, 12, DATE_ADD(CURRENT_DATE, INTERVAL 7 DAY), '全天', 25, 25);

INSERT INTO registrations (registration_no, patient_id, doctor_id, schedule_id, appointment_time_slot, register_time, status, fee, operator_id) VALUES
('R202606050001', 1, 1, 2, '08:30-09:00', TIMESTAMP(CURRENT_DATE, '08:30:00'), 'WAITING', 23.00, 3),
('R202606050002', 2, 2, 4, '09:00-09:30', TIMESTAMP(CURRENT_DATE, '09:00:00'), 'FINISHED', 18.00, 3),
('R202606050003', 3, 5, 8, '09:30-10:00', TIMESTAMP(CURRENT_DATE, '09:18:00'), 'WAITING', 22.00, 3),
('R202606050004', 4, 4, 7, '10:00-10:30', TIMESTAMP(CURRENT_DATE, '08:45:00'), 'CALLED', 20.00, 3),
('R202606050005', 5, 3, 6, '10:30-11:00', TIMESTAMP(CURRENT_DATE, '09:10:00'), 'CHECKING', 23.00, 3),
('R202606050006', 7, 6, 9, '11:00-11:30', TIMESTAMP(CURRENT_DATE, '10:20:00'), 'CANCELLED', 20.00, 3),
('R202606040001', 1, 1, 1, '14:00-14:30', TIMESTAMP(DATE_SUB(CURRENT_DATE, INTERVAL 1 DAY), '13:40:00'), 'FINISHED', 23.00, 3);

INSERT INTO medical_records
(registration_id, chief_complaint, present_illness, past_history, physical_sign, icd_code, diagnosis, advice,
 external_report_hospital, external_report_type, external_report_date, external_report_summary, external_report_conclusion, external_report_attachment, doctor_id) VALUES
(2, '右下腹疼痛一天', '患者诉右下腹隐痛，伴轻度恶心，无明显发热。', '否认重大手术史。', '腹部轻压痛，无反跳痛。', 'R10.401', '急性腹痛待查', '完善血常规和腹部彩超，清淡饮食，必要时复诊。',
 '市人民医院', '超声', DATE_SUB(CURRENT_DATE, INTERVAL 2 DAY), '外院腹部超声提示阑尾区未见明确包块。', '建议结合临床及血常规复查。', '', 2),
(7, '反复胸闷一周', '活动后胸闷加重，休息可缓解。', '高血压病史5年。', '血压148/92mmHg，心率82次/分。', 'I10.x00', '高血压病，冠心病待排', '低盐饮食，规律监测血压，一周后复诊。',
 '社区卫生服务中心', '心电图', DATE_SUB(CURRENT_DATE, INTERVAL 3 DAY), '外院心电图提示窦性心律。', 'ST-T轻度改变，建议专科进一步评估。', '', 1);

INSERT INTO examinations (examination_no, registration_id, doctor_id, item_name, request_note, result_text, status, request_time, complete_time) VALUES
('EX202606050001', 5, 3, '血常规', '贫血原因筛查', '', 'PENDING', TIMESTAMP(CURRENT_DATE, '09:30:00'), NULL),
('EX202606050002', 4, 4, '胸部DR', '咳嗽伴胸闷', '双肺纹理增多，未见明显实变影。', 'COMPLETED', TIMESTAMP(CURRENT_DATE, '09:05:00'), TIMESTAMP(CURRENT_DATE, '09:40:00')),
('EX202606040001', 7, 1, '心电图', '胸闷心悸评估', '窦性心律，ST-T轻度改变。', 'COMPLETED', TIMESTAMP(DATE_SUB(CURRENT_DATE, INTERVAL 1 DAY), '14:05:00'), TIMESTAMP(DATE_SUB(CURRENT_DATE, INTERVAL 1 DAY), '14:18:00'));

INSERT INTO prescriptions (prescription_no, registration_id, doctor_id, status, total_amount) VALUES
('RX202606050001', 2, 2, 'PAID', 38.00),
('RX202606050002', 7, 1, 'CREATED', 16.00),
('RX202606050003', 7, 1, 'REVIEWED', 44.00),
('RX202606050004', 2, 2, 'DISPENSED', 18.00);

INSERT INTO drug_categories (category_name, description) VALUES
('抗生素', '感染相关用药'),
('解热镇痛', '退热、止痛相关用药'),
('消化系统', '胃肠道相关用药'),
('心血管', '高血压、冠心病相关用药'),
('血液系统', '贫血和造血系统相关用药');

INSERT INTO drugs (drug_code, barcode, drug_name, category_id, specification, unit, purchase_price, sale_price, stock_quantity, warning_quantity, expiry_date) VALUES
('D001', '6900000000011', '阿莫西林胶囊', 1, '0.25g*24粒', '盒', 8.50, 12.00, 120, 20, DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR)),
('D002', '6900000000028', '布洛芬缓释胶囊', 2, '0.3g*20粒', '盒', 10.00, 16.00, 80, 20, DATE_ADD(CURRENT_DATE, INTERVAL 20 DAY)),
('D003', '6900000000035', '奥美拉唑肠溶胶囊', 3, '20mg*14粒', '盒', 9.00, 14.00, 8, 15, DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR)),
('D004', '6900000000042', '硝苯地平缓释片', 4, '20mg*30片', '盒', 15.00, 22.00, 45, 20, DATE_ADD(CURRENT_DATE, INTERVAL 8 MONTH)),
('D005', '6900000000059', '蒙脱石散', 3, '3g*10袋', '盒', 12.00, 18.00, 5, 10, DATE_ADD(CURRENT_DATE, INTERVAL 12 DAY)),
('D006', '6900000000066', '葡萄糖酸亚铁片', 5, '0.3g*60片', '瓶', 18.00, 26.00, 33, 12, DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR));

INSERT INTO pass_rules (rule_code, rule_type, drug_name, related_drug_name, patient_condition, warning_level, message) VALUES
('PASS001', 'ALLERGY', '阿莫西林胶囊', NULL, '青霉素过敏', 'BLOCK', '患者存在青霉素过敏风险，禁止开立阿莫西林类药品。'),
('PASS002', 'DOSE', '布洛芬缓释胶囊', NULL, '儿童', 'WARN', '儿童使用布洛芬需核对年龄和体重，避免超剂量。'),
('PASS003', 'COMBO', '阿莫西林胶囊', '布洛芬缓释胶囊', NULL, 'WARN', '抗生素与解热镇痛药联合使用时需确认感染指征和胃肠道风险。');

INSERT INTO prescription_items (prescription_id, drug_id, quantity, dosage, frequency, days, unit_price, amount) VALUES
(1, 1, 2, '一次1粒', '每日3次', 3, 12.00, 24.00),
(1, 3, 1, '一次1粒', '每日1次', 7, 14.00, 14.00),
(2, 2, 1, '一次1粒', '每日2次', 2, 16.00, 16.00),
(3, 4, 2, '一次1片', '每日1次', 14, 22.00, 44.00),
(4, 5, 1, '一次1袋', '每日3次', 3, 18.00, 18.00);

INSERT INTO stock_records (drug_id, change_type, quantity, before_quantity, after_quantity, related_no, operator_id) VALUES
(1, 'IN', 100, 20, 120, 'IN202605280001', 6),
(2, 'IN', 80, 0, 80, 'IN202605280002', 6),
(3, 'OUT', 1, 9, 8, 'RX202606050001', 6),
(4, 'IN', 45, 0, 45, 'IN202606050001', 6),
(5, 'OUT', 1, 6, 5, 'RX202606050004', 6),
(6, 'IN', 33, 0, 33, 'IN202606050002', 6);

INSERT INTO bills (bill_no, registration_id, patient_id, registration_fee, drug_fee, other_fee, total_amount, status) VALUES
('B202606050001', 1, 1, 23.00, 0.00, 0.00, 23.00, 'UNPAID'),
('B202606050002', 2, 2, 18.00, 56.00, 30.00, 104.00, 'PAID'),
('B202606050003', 3, 3, 22.00, 0.00, 0.00, 22.00, 'UNPAID'),
('B202606050004', 4, 4, 20.00, 0.00, 45.00, 65.00, 'PAID'),
('B202606050005', 5, 5, 23.00, 0.00, 25.00, 48.00, 'UNPAID'),
('B202606050006', 6, 7, 20.00, 0.00, 0.00, 20.00, 'CANCELLED'),
('B202606040001', 7, 1, 23.00, 44.00, 20.00, 87.00, 'REFUNDED');

INSERT INTO payments (payment_no, bill_id, amount, pay_method, cashier_id) VALUES
('PAY202606050001', 2, 104.00, '微信支付', 7),
('PAY202606050002', 4, 65.00, '现金', 7),
('PAY202606040001', 7, 87.00, '支付宝', 7);

INSERT INTO fee_statistics_daily (stat_date, department_id, registration_income, drug_income, total_income) VALUES
(DATE_SUB(CURRENT_DATE, INTERVAL 1 DAY), 3, 23.00, 44.00, 87.00),
(CURRENT_DATE, 3, 23.00, 0.00, 23.00),
(CURRENT_DATE, 29, 18.00, 56.00, 104.00),
(CURRENT_DATE, 16, 20.00, 0.00, 65.00),
(CURRENT_DATE, 7, 23.00, 0.00, 48.00),
(CURRENT_DATE, NULL, 84.00, 56.00, 240.00),
(DATE_ADD(CURRENT_DATE, INTERVAL 1 DAY), 41, 22.00, 0.00, 22.00);

INSERT INTO operation_logs (user_id, module, action, content, ip_address, created_at) VALUES
(3, '挂号管理', '新增挂号', '为王小兰创建今日心血管内科预约，时段 08:30-09:00。', '127.0.0.1', TIMESTAMP(CURRENT_DATE, '08:30:00')),
(4, '医生排班', '重新排班', '清空旧号源并生成全天排班。', '127.0.0.1', TIMESTAMP(CURRENT_DATE, '08:35:00')),
(9, '医生接诊', '叫号', '呼叫李建国进入呼吸内科诊室。', '127.0.0.1', TIMESTAMP(CURRENT_DATE, '09:00:00')),
(8, '检查管理', '开立检查', '为刘芳开立血常规检查。', '127.0.0.1', TIMESTAMP(CURRENT_DATE, '09:30:00')),
(6, '处方管理', '审核处方', '审核通过 RX202606050003。', '127.0.0.1', TIMESTAMP(CURRENT_DATE, '10:12:00')),
(7, '收费管理', '收费', '赵强账单 B202606050002 已完成微信支付。', '127.0.0.1', TIMESTAMP(CURRENT_DATE, '10:35:00')),
(1, '患者管理', '导出', '导出患者管理 CSV 测试数据。', '127.0.0.1', TIMESTAMP(CURRENT_DATE, '10:50:00'));

INSERT INTO audit_log_details (operation_log_id, user_id, module, action, business_key, field_name, old_value, new_value, change_reason, created_at) VALUES
(2, 4, '医生排班', '重新排班', '张明-' , '时段', '上午/下午', '全天', '排班规则调整', TIMESTAMP(CURRENT_DATE, '08:35:00')),
(5, 6, '处方管理', '审核处方', 'RX202606050003', '状态', '待审核', '待发药', '药师审核通过', TIMESTAMP(CURRENT_DATE, '10:12:00')),
(6, 7, '收费管理', '收费', 'B202606050002', '状态', '待缴费', '已缴费', '患者完成付款', TIMESTAMP(CURRENT_DATE, '10:35:00'));
