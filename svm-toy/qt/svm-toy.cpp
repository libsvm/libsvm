#include <qapplication.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qwidget.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <stdio.h>
#include <stdlib.h>
#include <list>
#include "../../svm.h"
#define DEFAULT_PARAM "-t 2 -c 100"
#define XLEN 500
#define YLEN 500

class ScribbleWindow : public QWidget
{

Q_OBJECT

public:
	ScribbleWindow();

protected:
	virtual void mousePressEvent( QMouseEvent* );
	virtual void paintEvent( QPaintEvent* );

private:
	QPixmap buffer;
	QPixmap icon1;
	QPixmap icon2;
	QPushButton button_change_icon;
	QPushButton button_run;
	QPushButton button_clear;
	QLineEdit input_line;
	struct point {
		int x, y;
		signed char value;
	};
	list<point> point_list;
	int current_value;
	const QPixmap& choose_icon(int v)
	{
		if(v==1) return icon1;
		else return icon2;
	}

private slots: 
	void button_change_icon_clicked()
	{
		current_value = -current_value;
		button_change_icon.setPixmap(choose_icon(current_value));
	}
	void button_clear_clicked()
	{
		point_list.clear();
		buffer.fill(black);
		paintEvent(NULL);
	}
	void button_run_clicked()
	{
		svm_parameter param;
		int i,j;	

		// default values
		param.kernel_type = RBF;
		param.degree = 3;
		param.gamma = 0.5;
		param.coef0 = 0;
		param.cache_size = 40;
		param.C = 1;
		param.eps = 1e-3;

		// parse options
		const char *p = input_line.text();

		while (1) {
			while (*p && *p != '-')
				p++;

			if (*p == '\0')
				break;

			p++;
			switch (*p++) {
				case 't':
					param.kernel_type = atoi(p);
					break;
				case 'd':
					param.degree = atof(p);
					break;
				case 'g':
					param.gamma = atof(p);
					break;
				case 'r':
					param.coef0 = atof(p);
					break;
				case 'm':
					param.cache_size = atof(p);
					break;
				case 'c':
					param.C = atof(p);
					break;
				case 'e':
					param.eps = atof(p);
					break;
			}
		}
	
		// build problem
		svm_problem prob;

		prob.l = point_list.size();
		svm_node *x_space = new svm_node[3 * prob.l];
		prob.x = new svm_node *[prob.l];
		prob.y = new signed char[prob.l];
		i = 0;
		for (list <point>::iterator q = point_list.begin(); q != point_list.end(); q++, i++)
		{
			x_space[3 * i].index = 1;
			x_space[3 * i].value = (double) q->x / XLEN;
			x_space[3 * i + 1].index = 2;
			x_space[3 * i + 1].value = (double) q->y / YLEN;
			x_space[3 * i + 2].index = -1;
			prob.x[i] = &x_space[3 * i];
			prob.y[i] = q->value;
		}

		// build model & classify
		svm_model *model = svm_train(&prob, &param);
		svm_node x[3];
		x[0].index = 1;
		x[1].index = 2;
		x[2].index = -1;

		QPainter windowpainter,painter;
		windowpainter.begin(this);
		painter.begin(&buffer);

		for (i = 0; i < XLEN; i++)
			for (j = 0; j < YLEN - 25; j++) {
				x[0].value = (double) i / XLEN;
				x[1].value = (double) j / YLEN;
				double d = svm_classify(model, x);
				QRgb color;
				if (d > 1)
					color = qRgb(0, 150, 150);
				else if (d > 0)
					color = qRgb(0, 100, 100);
				else if (d > -1)
					color = qRgb(100, 100, 0);
				else
					color = qRgb(150, 150, 0);

				painter.setPen(color);
				windowpainter.setPen(color);
				painter.drawPoint(i,j);
				windowpainter.drawPoint(i,j);
		}

		svm_destroy_model(model);
		delete[] x_space;
		delete[] prob.x;
		delete[] prob.y;

		for(list<point>::iterator p = point_list.begin(); p != point_list.end();p++)
		{
			const QPixmap& icon = choose_icon(p->value);
			windowpainter.drawPixmap(p->x,p->y,icon);
			painter.drawPixmap(p->x,p->y,icon);
		}

		windowpainter.end();
		painter.end();
	}
};

#include "svm-toy.moc"

ScribbleWindow::ScribbleWindow()
:button_change_icon(this)
,button_run("Run",this)
,button_clear("Clear",this)
,input_line(this)
,current_value(1)
{
	buffer.resize(XLEN,YLEN);
	buffer.fill(black);

	QObject::connect(&button_change_icon, SIGNAL(pressed()), this,
			 SLOT(button_change_icon_clicked()));
	QObject::connect(&button_run, SIGNAL(pressed()), this,
			 SLOT(button_run_clicked()));
	QObject::connect(&button_clear, SIGNAL(pressed()), this,
			 SLOT(button_clear_clicked()));
	QObject::connect( &input_line, SIGNAL(returnPressed()), this,
			 SLOT(button_run_clicked()));

  	// don't blank the window before repainting
	setBackgroundMode( NoBackground );
  
	icon1.resize(4,4);
	icon2.resize(4,4);
	QPainter painter;
	painter.begin(&icon1);
	painter.fillRect(0,0,4,4,QBrush(qRgb(0,200,200)));
	painter.end();

	painter.begin(&icon2);
	painter.fillRect(0,0,4,4,QBrush(qRgb(200,200,0)));
	painter.end();

	button_change_icon.setGeometry( 0, YLEN-25, 50, 25 );
	button_run.setGeometry( 50, YLEN-25, 50, 25 );
	button_clear.setGeometry( 100, YLEN-25, 50, 25 );
	input_line.setGeometry( 150, YLEN-25, 350, 25);
	
	input_line.setText(DEFAULT_PARAM);
	button_change_icon.setPixmap(icon1);
}

void ScribbleWindow::mousePressEvent( QMouseEvent* event )
{
	const QPixmap& icon = choose_icon(current_value);
	QPainter painter;
	painter.begin( this );
	painter.drawPixmap(event->x(), event->y(), icon);
	painter.end();

	painter.begin( &buffer );
	painter.drawPixmap(event->x(), event->y(), icon);
	painter.end();

	point p = {event->x(), event->y(), current_value};
	point_list.push_back(p);
}

void ScribbleWindow::paintEvent( QPaintEvent* )
{
	// copy the image from the buffer pixmap to the window
	bitBlt( this, 0, 0, &buffer );
}

int main( int argc, char* argv[] )
{
	QApplication myapp( argc, argv );

	ScribbleWindow* mywidget = new ScribbleWindow();
	mywidget->setGeometry( 100, 100, XLEN, YLEN );

	myapp.setMainWidget( mywidget );
	mywidget->show();
	return myapp.exec();
}
