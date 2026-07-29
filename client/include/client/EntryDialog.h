#pragma once

#include <QDialog>
#include <QString>

namespace hospital { namespace client {

class EntryDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Choice {
        None,
        PatientLogin,
        PatientAppointment,
        StaffLogin
    };

    explicit EntryDialog(QWidget* parent = nullptr);
    Choice choice() const;
    QString staffUsername() const;

private:
    Choice m_choice = Choice::None;
    QString m_staffUsername = "admin";
};

}} // namespace hospital::client
