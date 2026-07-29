#pragma once

#include <QString>
#include <QStringList>

namespace hospital { namespace client { namespace DepartmentCatalog {

QStringList categories();
QStringList specialties(const QString& category);
QStringList clinics(const QString& category, const QString& specialty);
QStringList leavesForCategory(const QString& category);
QString categoryFor(const QString& department);
QString specialtyFor(const QString& department);
QString clinicFor(const QString& department);
QString firstClinic(const QString& category, const QString& specialty);
QString recommendedClinicForSymptoms(const QString& text);

}}} // namespace hospital::client::DepartmentCatalog
