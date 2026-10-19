#include "gfxtimelabel.h"

#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QTransform>
#include <QtMath>

#include "gfxstyle.h"

GfxTimeLabel::GfxTimeLabel(QWidget *parent)
    : QWidget(parent)
    , m_text(QString::fromLatin1(GfxStyle::TimeLabelText))
{
    m_font.setFamily(GfxStyle::LabelFontFamily);
    m_font.setStyleHint(GfxStyle::LabelFontStyleHint);
    m_font.setPointSizeF(GfxStyle::LabelFontPointSize);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    refreshSize();
}

void GfxTimeLabel::SetFontSize(int size)
{
    m_font.setPointSizeF(size);
    refreshSize();
    update();
}

void GfxTimeLabel::SetText(const QString &text)
{
    if (m_text == text)
        return;

    m_text = text;
    m_rotatedTextPixmap = QPixmap();
    refreshSize();
    update();
}

void GfxTimeLabel::refreshSize()
{
    QFontMetrics metrics(m_font, this);
    const int padding = GfxStyle::TimeLabelPadding;
    setFixedSize(metrics.height() + padding,
                 metrics.horizontalAdvance(m_text) + padding);
}

int GfxTimeLabel::rotatedTextWidth() const
{
    return QFontMetrics(m_font, this).height();
}

void GfxTimeLabel::setCaptureScale(qreal scale)
{
    const qreal captureScale = scale > 0 ? scale : 0;
    if (m_captureScale == captureScale)
        return;

    m_captureScale = captureScale;
    update();
}

void GfxTimeLabel::paintEvent(QPaintEvent *)
{
    updateTextPixmap();
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_rotatedTextPixmap);
}

void GfxTimeLabel::updateTextPixmap()
{
    const QString text = m_text;
    QFontMetrics metrics(m_font, this);
    const int padding = GfxStyle::TimeLabelPadding;
    const QSize textSize(metrics.horizontalAdvance(text) + padding,
                         metrics.height());
    const QColor textColor = palette().color(QPalette::WindowText);
    const qreal pixelRatio =
        this->devicePixelRatioF() *
        (m_captureScale > 0 ? m_captureScale : 1.0);
    if (!m_rotatedTextPixmap.isNull() && m_cachedFont == m_font &&
        m_cachedColor == textColor && m_cachedTextSize == textSize &&
        qFuzzyCompare(m_cachedPixelRatio, pixelRatio)) {
        return;
    }

    const QSize pixelSize(qCeil(textSize.width() * pixelRatio),
                          qCeil(textSize.height() * pixelRatio));
    QPixmap horizontalTextPixmap(pixelSize);
    horizontalTextPixmap.fill(Qt::transparent);
    {
        QPainter textPainter(&horizontalTextPixmap);
        textPainter.scale(pixelRatio, pixelRatio);
        textPainter.setRenderHint(QPainter::TextAntialiasing);
        textPainter.setFont(m_font);
        textPainter.setPen(textColor);
        textPainter.drawText(QPoint(padding / 2, metrics.ascent()), text);
    }

    m_rotatedTextPixmap =
        horizontalTextPixmap.transformed(QTransform().rotate(-90),
                                         Qt::SmoothTransformation);
    m_rotatedTextPixmap.setDevicePixelRatio(pixelRatio);
    m_cachedFont = m_font;
    m_cachedColor = textColor;
    m_cachedTextSize = textSize;
    m_cachedPixelRatio = pixelRatio;
}
