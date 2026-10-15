#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QLabel;
class QPushButton;

/**
 * @brief 设置手动控制参数弹窗 (对标参考图2，仅保留设置速度)
 */
class ManualControlConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ManualControlConfigDialog(QWidget *parent = nullptr);
    ~ManualControlConfigDialog() override = default;

    static constexpr double kGlobalMaxLinearSpeed  = 1.000; ///< 全局最大速度 1.000 m/s
    static constexpr double kGlobalMaxAngularSpeed = 90.000; ///< 全局最大转速度 90.000 °/s

private slots:
    void onConfirm();

private:
    void setupUI();

private:
    QDoubleSpinBox *m_linearSpin  = nullptr;
    QDoubleSpinBox *m_angularSpin = nullptr;
    QPushButton    *m_confirmBtn  = nullptr;
};
