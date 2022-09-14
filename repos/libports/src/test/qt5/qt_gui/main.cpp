/*
 * \brief   QtGui test
 * \author  Christian Prochaska
 * \date    2022-09-12
 */

#include <QDebug>
#include <QtGui>
#include <QtWidgets>

class Test_widget : public QWidget
{
	Q_OBJECT

	private:

		int _c = 0;

	private Q_SLOTS:

		virtual void _timer()
		{
			QElapsedTimer t;
			t.start();
			qDebug() << "start";
			static constexpr int frames = 10000;
			for (int i = 0; i < frames; i++) repaint();
			qDebug() << "end";
			qDebug() << frames << "frames in" << t.elapsed() << "ms ="
			         << (frames * 1000) / t.elapsed() << "fps";
		}

	protected:

		void paintEvent(QPaintEvent *event)
		{
			QPainter painter(this);
			painter.fillRect(event->rect(), QColor(_c, _c, _c));

			_c++;
			if (_c == 256) _c = 0;
		}

	public:

		Test_widget() : QWidget()
		{
			resize(400, 400);
			show();
			QTimer::singleShot(1000, this, SLOT(_timer()));
		}
		
};

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	Test_widget w;

	return app.exec();
}

#include "main.moc"
