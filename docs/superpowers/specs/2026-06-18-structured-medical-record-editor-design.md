# Structured Medical Record Editor Design

## Goal

Turn the current simple consultation form into a structured outpatient medical record editor with department templates, ICD diagnosis selection, history reuse, printing, and PDF export.

## Scope

This optimization point covers doctor consultation and patient record display. It does not implement the later role-permission admin page, examination dictionary/workstation, or refund approval workflow.

## Business Rules

- Doctors record outpatient medical records with structured fields: chief complaint, present illness, past history, physical examination, diagnosis, and advice.
- The editor provides templates for internal medicine, orthopedics, and pediatrics.
- Selecting a template fills empty structured fields with standard prompts.
- The editor provides an ICD diagnosis dropdown. Selecting an ICD item writes the normalized diagnosis text into the diagnosis field.
- Saving a consultation persists all structured fields to `medical_records`.
- Patient record archive displays structured fields so revisit doctors can review prior visits.
- Doctors can load the selected patient's latest historical medical record into the current editor.
- Doctors can print or export the current editor content as PDF through Qt printing support.

## Data Model

Use the existing `medical_records` columns:

- `chief_complaint`
- `present_illness`
- `past_history`
- `physical_sign`
- `diagnosis`
- `advice`

Add one lightweight compatibility field:

- `icd_code`

For existing databases, `DatabaseManager::ensureCompatibilitySchema()` adds missing medical record columns with idempotent `ALTER TABLE` statements.

## Client Behavior

The doctor consultation dialog gains:

- Template dropdown: `内科模板`, `骨科模板`, `儿科模板`.
- ICD dropdown with common outpatient diagnoses.
- Structured text areas for present illness, past history, and physical examination.
- Buttons: `套用模板`, `调取历史病历`, `打印病历`, `导出PDF`.

History loading uses data already present in the consultation/patient record row when available, and the server exposes structured fields in consultation lists so revisits can preload them.

## Server Behavior

`ConsultationService::saveConsultationInDatabase()` reads and writes structured fields and ICD code. `ConsultationService::list` returns structured fields for the active consultation page. `PatientRecordService` update/list/delete also preserves those fields.

Demo mode mirrors the same fields in `DemoRepository`.

## Verification

Add a focused source/schema test covering:

- Consultation UI includes template, ICD, history, print, and PDF controls.
- Consultation requests send `现病史`, `既往史`, `体格检查`, and `ICD编码`.
- Server saves and lists `present_illness`, `past_history`, `physical_sign`, and `icd_code`.
- Patient record archive exposes the same structured fields.
- CMake links Qt PrintSupport.
