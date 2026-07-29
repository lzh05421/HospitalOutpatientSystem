CREATE TABLE IF NOT EXISTS patient_users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    password_salt VARCHAR(64) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

ALTER TABLE patients ADD COLUMN user_id BIGINT NULL AFTER patient_no;
ALTER TABLE patients ADD COLUMN username VARCHAR(64) NULL AFTER patient_no;
ALTER TABLE patients ADD COLUMN password_hash VARCHAR(255) NULL AFTER username;
ALTER TABLE patients ADD COLUMN password_salt VARCHAR(64) NULL AFTER password_hash;
ALTER TABLE patients ADD COLUMN relationship VARCHAR(32) DEFAULT '本人' AFTER phone;
ALTER TABLE patients ADD UNIQUE KEY uk_patients_username (username);
ALTER TABLE patients ADD UNIQUE KEY uk_patients_user_id_card (user_id, id_card);
ALTER TABLE patients ADD CONSTRAINT fk_patients_user FOREIGN KEY (user_id) REFERENCES patient_users(id);
ALTER TABLE registrations ADD COLUMN user_id BIGINT NULL AFTER registration_no;
ALTER TABLE bills ADD COLUMN user_id BIGINT NULL AFTER bill_no;

UPDATE patients
SET username = COALESCE(NULLIF(phone, ''), patient_no),
    password_salt = patient_no,
    password_hash = SHA2(CONCAT(password_salt, ':123456'), 256)
WHERE username IS NULL OR username = '' OR password_hash IS NULL OR password_hash = '';

INSERT IGNORE INTO patient_users (username, password_salt, password_hash)
SELECT DISTINCT username, password_salt, password_hash
FROM patients
WHERE username IS NOT NULL AND username <> '';

UPDATE patients p
JOIN patient_users u ON u.username = p.username
SET p.user_id = u.id
WHERE p.user_id IS NULL;

UPDATE registrations r
JOIN patients p ON p.id = r.patient_id
SET r.user_id = p.user_id
WHERE r.user_id IS NULL;

UPDATE bills b
JOIN patients p ON p.id = b.patient_id
SET b.user_id = p.user_id
WHERE b.user_id IS NULL;
