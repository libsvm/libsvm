#include <gtk/gtk.h>
#include <stdio.h>
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
	double x, y;
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
	pixmap = gdk_pixmap_new(draw_main->window,XLEN,YLEN,-1);
	gdk_gc_set_foreground(gc,&colors[0]);
	gdk_draw_rectangle(pixmap,gc,TRUE,0,0,XLEN,YLEN);
	gtk_entry_set_text(GTK_ENTRY(entry_option),DEFAULT_PARAM);
}

void redraw_area(GtkWidget* widget, int x, int y, int w, int h)
{
	gdk_draw_pixmap(widget->window,
			gc,
			pixmap,
			x,y,x,y,w,h);
}

void draw_point(const point& p)
{
	gdk_gc_set_foreground(gc,(p.value==1)?(&colors[1]):(&colors[6]));
	gdk_draw_rectangle(pixmap, gc, TRUE,int(p.x*XLEN),int(p.y*YLEN),4,4);
	gdk_draw_rectangle(draw_main->window, gc, TRUE,int(p.x*XLEN),int(p.y*YLEN),4,4);
}

void draw_all_points()
{
	for(list<point>::iterator p = point_list.begin(); p != point_list.end();p++)
		draw_point(*p);
}

void clear_all()
{
	point_list.clear();
	gdk_gc_set_foreground(gc,&colors[0]);
	gdk_draw_rectangle(pixmap,gc,TRUE,0,0,XLEN,YLEN);
	redraw_area(draw_main,0,0,XLEN,YLEN);
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
	// guard
	if(point_list.empty()) return;

	svm_parameter param;
	int i,j;	

	// default values
	param.svm_type = C_SVC;
	param.kernel_type = RBF;
	param.degree = 3;
	param.gamma = 0.5;
	param.coef0 = 0;
	param.nu = 0.5;
	param.cache_size = 40;
	param.C = 1;
	param.eps = 1e-3;
	param.p = 0.5;

	// parse options
	const char *p = gtk_entry_get_text(GTK_ENTRY(entry_option));

	while (1) {
		while (*p && *p != '-')
			p++;

		if (*p == '\0')
			break;

		p++;
		switch (*p++) {
			case 's':
				param.svm_type = atoi(p);
				break;
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
			case 'n':
				param.nu = atof(p);
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
			case 'p':
				param.p = atof(p);
				break;
		}
	}
	
	// build problem
	svm_problem prob;

	prob.l = point_list.size();
	svm_node *x_space = new svm_node[3 * prob.l];
	prob.x = new svm_node *[prob.l];
	prob.y = new double[prob.l];

	i = 0;
	for (list <point>::iterator q = point_list.begin(); q != point_list.end(); q++, i++)
	{
		x_space[3 * i].index = 1;
		x_space[3 * i].value = q->x;
		x_space[3 * i + 1].index = 2;
		x_space[3 * i + 1].value = q->y;
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
		for (j = 0; j < YLEN; j++) {
			x[0].value = (double) i / XLEN;
			x[1].value = (double) j / YLEN;
			double d;
			svm_classify(model, x, 1, &d);
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

	draw_all_points();
}

void
on_button_clear_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
	clear_all();
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
	point p = {(double)event->x/XLEN, (double)event->y/YLEN, current_value};
	point_list.push_back(p);
	draw_point(p);
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

GtkWidget *fileselection;
enum { SAVE, LOAD } fileselection_flag;

void show_fileselection()
{
	fileselection = create_fileselection();
	gtk_signal_connect_object(
		GTK_OBJECT(GTK_FILE_SELECTION(fileselection)->ok_button),
		"clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
		(GtkObject *) fileselection);
	
	gtk_signal_connect_object (GTK_OBJECT
		(GTK_FILE_SELECTION(fileselection)->cancel_button),
		"clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
		(GtkObject *) fileselection);

	gtk_widget_show(fileselection);
}

void
on_button_save_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
	fileselection_flag = SAVE;
	show_fileselection();
}


void
on_button_load_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
	fileselection_flag = LOAD;
	show_fileselection();
}

void
on_filesel_ok_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
	gtk_widget_hide(fileselection);
	const char *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileselection));

	if(fileselection_flag == SAVE)
	{
		FILE *fp = fopen(filename,"w");
		if(fp)
		{
			for(list<point>::iterator p = point_list.begin(); p != point_list.end();p++)
				fprintf(fp,"%d 1:%f 2:%f\n", p->value, p->x, p->y);
			fclose(fp);
		}

	}
	else if(fileselection_flag == LOAD)
	{
		FILE *fp = fopen(filename,"r");
		if(fp)
		{
			clear_all();
			char buf[4096];
			while(fgets(buf,sizeof(buf),fp))
			{
				int v;
				double x,y;
				if(sscanf(buf,"%d%*d:%lf%*d:%lf",&v,&x,&y)!=3)
					break;													
				point p = {x,y,v};
				point_list.push_back(p);
			}
			fclose(fp);
			draw_all_points();
		}				
	}
}

void
on_fileselection_destroy               (GtkObject       *object,
                                        gpointer         user_data)
{
}

void
on_filesel_cancel_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
}
