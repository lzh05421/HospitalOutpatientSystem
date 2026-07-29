# Structured Medical Record Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add structured outpatient medical record editing with templates, ICD diagnosis selection, history reuse, print, and PDF export.

**Architecture:** Extend the existing `ConsultationPage`, `ConsultationService`, `PatientRecordService`, and `DemoRepository` rather than creating a parallel record module. Persist structured fields in `medical_records`, expose them in existing list responses, and add Qt PrintSupport for printing/PDF.

**Tech Stack:** C++17, Qt Widgets, Qt PrintSupport, Qt SQL, CMake, CTest.

---

## File Structure

- Modify `client/src/pages/ConsultationPage.cpp`: add structured editor controls, templates, ICD dropdown, history fill, print/PDF helpers, and payload fields.
- Modify `client/CMakeLists.txt`: link `Qt::PrintSupport`.
- Modify `server/src/modules/ConsultationService.cpp`: save/list structured fields and ICD code.
- Modify `server/src/modules/PatientRecordService.cpp`: update/list/delete structured fields and ICD code.
- Modify `server/src/DemoRepository.cpp`: preserve and show structured fields in demo mode.
- Modify `server/src/DatabaseManager.cpp`: add compatibility columns for existing databases.
- Modify `database/schema.sql`: add `icd_code` to `medical_records`.
- Modify `client/src/pages/ModulePage.cpp`: show structured medical record columns in patient archive.
- Create `tests/structured_medical_record_source_tests.cpp`: source/schema contract test.
- Modify `tests/CMakeLists.txt`: register the new test.

## Task 1: Red Test

- [ ] Add `tests/structured_medical_record_source_tests.cpp` checking for template/ICD/history/print/PDF controls, structured payload fields, structured SQL fields, PrintSupport linking, and schema support.
- [ ] Register the test in `tests/CMakeLists.txt`.
- [ ] Run `cmake --build build-codex --target structured_medical_record_source_tests`.
- [ ] Run `ctest --test-dir build-codex -R structured_medical_record_source_tests --output-on-failure`.
- [ ] Confirm it fails before implementation.

## Task 2: Server Persistence

- [ ] Add `icd_code` to `database/schema.sql`.
- [ ] Add idempotent `ALTER TABLE medical_records ADD COLUMN ...` statements in `DatabaseManager.cpp`.
- [ ] Update `ConsultationService.cpp` insert/update SQL to bind `现病史`, `既往史`, `体格检查`, and `ICD编码`.
- [ ] Update consultation list SQL to return `现病史`, `既往史`, `体格检查`, and `ICD编码`.
- [ ] Update `PatientRecordService.cpp` update/list/delete SQL and audit details for the structured fields.

## Task 3: Demo Mode

- [ ] Update demo patient record rows with `现病史`, `既往史`, `体格检查`, and `ICD编码`.
- [ ] Update `saveConsultation`, `updatePatientRecord`, and delete behavior to preserve or clear structured fields.

## Task 4: Client Editor

- [ ] Link `Qt::PrintSupport` in `client/CMakeLists.txt`.
- [ ] Add template and ICD dropdown controls to `ConsultationPage.cpp`.
- [ ] Add text editors for `现病史`, `既往史`, and `体格检查`.
- [ ] Send structured fields in the consultation save payload.
- [ ] Add history loading from the current row's existing structured fields.
- [ ] Add print and PDF export using `QPrinter` and `QTextDocument`.
- [ ] Update patient record preferred columns in `ModulePage.cpp`.

## Task 5: Verification

- [ ] Run `ctest --test-dir build-codex -R structured_medical_record_source_tests --output-on-failure`.
- [ ] Run `cmake --build build-codex --target hospital_server_core hospital_client`.
- [ ] Run `ctest --test-dir build-codex -E hiredis-test --output-on-failure`.
- [ ] Report any third-party-only failures separately.
