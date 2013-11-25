#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_MODEL (my_model_get_type ())
#define MY_MODEL(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), MY_TYPE_MODEL, MyModel))

typedef struct _MyModel      MyModel;
typedef struct _MyModelClass MyModelClass;

struct _MyModel
{
  GObject parent_instance;
};

struct _MyModelClass
{
  GObjectClass parent_class;
};

GType        my_model_get_type (void);
MyModel     *my_model_new      (gint size);
gint         my_model_get_size (MyModel *model);
const gchar *my_model_get_item (MyModel *model, gint n);

G_END_DECLS
