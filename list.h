#pragma once

#include <gtk/gtk.h>
#include "model.h"

G_BEGIN_DECLS

#define MY_TYPE_LIST (my_list_get_type ())
#define MY_LIST(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), MY_TYPE_LIST, MyList))

typedef struct _MyList      MyList;
typedef struct _MyListClass MyListClass;

struct _MyList
{
  GtkContainer parent_instance;
};

struct _MyListClass
{
  GtkContainerClass parent_class;
};

GType      my_list_get_type  (void);
MyList *   my_list_new       (void);

typedef GtkWidget* (*CreateRowFunc) (void);
typedef void       (*ConnectRowFunc) (MyModel   *model,
                                      gint       n,
                                      GtkWidget *row);

void       my_list_set_model (MyList         *list,
                              MyModel        *model,
                              CreateRowFunc   create_row,
                              ConnectRowFunc  connect_row);

G_END_DECLS
