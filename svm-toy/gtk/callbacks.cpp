#include <gtk/gtk.h>
#include <list>
#include "callbacks.h"
#include "interface.h"
#include "../../svm.h"
#define DEFAULT_PARAM "-t 2 -c 100"
#define XLEN 500
#define YLEN 500

GdkColor colors[] = 
{
	{0,0,0},
	{0,0,200<<8,200<<8},
	{0,0,150<<8,150<<8},
	{0,0,100<<8,100<<8},
	{0,100<<8,100<<8,0},
	{0,150<<8,150<<8,0},
	{0,200<<8,200<<8,0},
};

GdkGC *gc;
GdkPixmap *pixmap;
extern "C" GtkWidget *draw_main;
GtkWidget *draw_main;
extern "C" GtkWidget *entry_option;
GtkWidget *entry_option;

typedef struct {
	int x, y;
	signed char value;
} point;
list<point> point_list;
int current_value = 1;

extern "C" void svm_toy_initialize()
{
	gboolean success[7];

	gdk_colormap_alloc_colors(
		gdk_colormap_get_system(),
		colors,
		7,
		FALSE,
		TRUE,
		success);

	gc = gdk_gc_new(draw_main->window);
	pixmap = gdk_pixmap_new(draw_main->window,XLEN,YLEN-25,-1);
	gdk_gc_set_foreground(gc,&colors[0]);
	gdk_draw_rectangle(pixmap,gc,TRUE,0,0,XLEN,YLEN-25);
	gtk_entry_set_text(GTK_ENTRY(entry_option),DEFAULT_PARAM);
}

void redraw_area(GtkWidget* widget, int x, int y, int w, int h)
{
	gdk_draw_pixmap(widget->window,
			gc,
			pixmap,
			x,y,x,y,w,h);
}

void
on_button_change_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
	current_value = -current_value;
}

void
on_button_run_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
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
	const char *p = gtk_entry_get_text(GTK_ENTRY(entry_option));

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
	
	for (i = 0; i < XLEN; i++) 
		for (j = 0; j < YLEN-25; j++){
			x[0].value = (double) i / XLEN;
			x[1].value = (double) j / YLEN;
			double d = svm_classify(model, x);
			GdkColor *color;
			if (d > 1)
				color = &colors[2];
			else if (d > 0)
				color = &colors[3];
			else if (d > -1)
				color = &colors[4];
			else
				color = &colors[5];

			gdk_gc_set_foreground(gc,color);
			gdk_draw_point(pixmap,gc,i,j);
			gdk_draw_point(draw_main->window,gc,i,j);
		}

	svm_destroy_model(model);
	delete[] x_space;
	delete[] prob.x;
	delete[] prob.y;

	for(list<point>::iterator p = point_list.begin(); p != point_list.end();p++)
	{
		gdk_gc_set_foreground(gc,
			(p->value==1)?(&colors[1]):(&colors[6]));

		gdk_draw_rectangle(pixmap, gc, TRUE,p->x,p->y,4,4);
		gdk_draw_rectangle(draw_main->window, gc, TRUE,p->x,p->y,4,4);
	}
}

void
on_button_clear_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
	point_list.clear();
	gdk_gc_set_foreground(gc,&colors[0]);
	gdk_draw_rectangle(pixmap,gc,TRUE,0,0,XLEN,YLEN-25);
	redraw_area(draw_main,0,0,XLEN,YLEN-25);
}

void
on_window1_destroy                     (GtkObject       *object,
                                        gpointer         user_data)
{
	gtk_exit(0);
}

gboolean
on_draw_main_button_press_event        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
	gdk_gc_set_foreground(gc,
		(current_value==1)?(&colors[1]):(&colors[6]));

	gdk_draw_rectangle(pixmap, gc, TRUE,(int)event->x,(int)event->y,4,4);
	gdk_draw_rectangle(widget->window, gc, TRUE,(int)event->x,(int)event->y,4,4);
	
	point p = {(int)event->x, (int)event->y, current_value};
	point_list.push_back(p);

	return FALSE;
}

gboolean
on_draw_main_expose_event              (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data)
{
	redraw_area(widget,
		    event->area.x, event->area.y,
		    event->area.width, event->area.height);
	return FALSE;
}
