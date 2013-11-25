#include "model.h"

typedef struct {
  gchar **items;
  gint size;
} MyModelPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MyModel, my_model, G_TYPE_OBJECT)

static void
my_model_init (MyModel *model)
{
}

static void
my_model_finalize (GObject *object)
{
  MyModelPrivate *priv = my_model_get_instance_private (MY_MODEL (object));

  g_strfreev (priv->items);

  G_OBJECT_CLASS (my_model_parent_class)->finalize (object);
}

static void
my_model_class_init (MyModelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = my_model_finalize;
}

MyModel *
my_model_new (gint size)
{
  MyModel *model;
  MyModelPrivate *priv;
  gint i;

  model = g_object_new (MY_TYPE_MODEL, NULL);
  priv = my_model_get_instance_private (model);
  priv->size = size;
  priv->items = g_new (gchar *, size + 1);
  for (i = 0; i < size; i++)
    priv->items[i] = g_strdup_printf ("Item %d", i);
  priv->items[size] = NULL;

  return model;
}

gint
my_model_get_size (MyModel *model)
{
  MyModelPrivate *priv = my_model_get_instance_private (model);

  return priv->size;
}

const gchar *
my_model_get_item (MyModel *model, gint n)
{
  MyModelPrivate *priv = my_model_get_instance_private (model);

  if (n < 0 || n >= priv->size)
    return NULL;

  return priv->items[n];
}
