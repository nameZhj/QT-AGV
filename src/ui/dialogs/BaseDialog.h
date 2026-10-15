#pragma once

#include <QDialog>

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QDialogButtonBox;

/**
 * @brief 业务通用对话框基类，统一窗口标志、边距、确定/取消按钮栏与居中控制
 */
class BaseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BaseDialog(const QString &title = QString(), QWidget *parent = nullptr);
    ~BaseDialog() override = default;

    /**
     * @brief 获取弹窗根纵向布局
     */
    QVBoxLayout* rootLayout() const { return m_rootLayout; }

    /**
     * @brief 创建标准“确定 / 取消”操作按钮栏 (返回水平布局)
     */
    QHBoxLayout* createButtonBox(const QString &okText = "确定", const QString &cancelText = "取消");

    /**
     * @brief 居中显示于父窗口
     */
    void centerOnParent();

protected:
    void showEvent(QShowEvent *event) override;

    QPushButton *m_okBtn     = nullptr;
    QPushButton *m_cancelBtn = nullptr;

private:
    QVBoxLayout *m_rootLayout = nullptr;
};
