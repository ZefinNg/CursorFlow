#pragma once

#include <QObject>

// 首次启动隐私授权门（PRD 5.4 / 04 号文档 §2）：
// 未同意前不启动采集，选择结果经 QSettings 持久化，重启后不再询问。
class PrivacyGate : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool accepted READ accepted NOTIFY acceptedChanged)

public:
    explicit PrivacyGate(QObject* parent = nullptr);

    bool accepted() const { return accepted_; }

    Q_INVOKABLE void accept();
    Q_INVOKABLE void decline(); // 暂不使用：保持未授权状态（04 号文档 §2）

signals:
    void acceptedChanged(bool accepted);

private:
    void loadAccepted();

    bool accepted_ = false;
};
