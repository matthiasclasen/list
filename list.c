
#include <math.h>
#include "list.h"
#include "model.h"

typedef struct {
  MyModel *model;
  CreateRowFunc create_row;
  ConnectRowFunc connect_row;
  GList *children;
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  gdouble height;
  gdouble average_row_height;
} MyListPrivate;

enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP];

G_DEFINE_TYPE_WITH_CODE (MyList, my_list, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (MyList)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static void
my_list_init (MyList *mylist)
{
  MyListPrivate *priv = my_list_get_instance_private (mylist);

  gtk_widget_set_has_window (GTK_WIDGET (mylist), FALSE);

  priv->average_row_height = 20;
}

static void
my_list_finalize (GObject *object)
{
  MyListPrivate *priv = my_list_get_instance_private (MY_LIST (object));

  g_object_unref (priv->model);

  G_OBJECT_CLASS (my_list_parent_class)->finalize (object);
}

static void
my_list_forall (GtkContainer *container,
                gboolean      include_internals,
                GtkCallback   callback,
                gpointer      callback_data)
{
  MyListPrivate *priv = my_list_get_instance_private (MY_LIST (container));

  if (include_internals)
    {
      GList *l;
      for (l = priv->children; l; l = l->next)
        callback (l->data, callback_data);
    }
}

static void
my_list_get_preferred_width (GtkWidget *widget,
                             gint      *minimum,
                             gint      *natural)
{
  MyListPrivate *priv = my_list_get_instance_private (MY_LIST (widget));
  GList *l;
  gint min, nat;

  if (priv->children)
    {
      min = 0;
      nat = 0;
      for (l = priv->children; l; l = l->next)
        {
          GtkWidget *child = l->data;
          gint child_min, child_nat;

          gtk_widget_get_preferred_width (child, &child_min, &child_nat);

          min = MAX (min, child_min);
          nat = MAX (nat, child_nat);
        }
    }
  else
    {
      min = nat = 100;
    }

  if (minimum)
    *minimum = min;
  if (natural)
    *natural = nat;
}

static void
my_list_get_preferred_width_for_height (GtkWidget *widget,
                                        gint       height,
                                        gint      *minimum,
                                        gint      *natural)
{
  my_list_get_preferred_width (widget, minimum, natural);
}

static void
my_list_get_preferred_height_for_width (GtkWidget *widget,
                                        gint       width,
                                        gint      *minimum,
                                        gint      *natural)
{
  MyListPrivate *priv = my_list_get_instance_private (MY_LIST (widget));
  GList *l;
  gint min, nat;
  gint n_children;
  gint model_size;

  if (priv->children)
    {
      min = 0;
      nat = 0;
      n_children = 0;
      for (l = priv->children; l; l = l->next)
        {
          GtkWidget *child = l->data;
          gint child_min, child_nat;

          gtk_widget_get_preferred_height_for_width (child, width, &child_min, &child_nat);

          min += child_min;
          nat += child_nat;
          n_children += 1;
        }
    }
  else
    {
      min = nat = 20;
      n_children = 1;
    }

  model_size = my_model_get_size (priv->model);

  if (minimum)
    *minimum = min * model_size / n_children;
  if (natural)
    *natural = nat * model_size / n_children;
}

static void
my_list_get_preferred_height (GtkWidget *widget,
                              gint      *minimum,
                              gint      *natural)
{
  gint width;

  my_list_get_preferred_width (widget, &width, NULL);
  my_list_get_preferred_height_for_width (widget, width, minimum, natural);
}

static GtkSizeRequestMode
my_list_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
set_hadjustment_values (MyList *mylist)
{
  GtkWidget *widget = GTK_WIDGET (mylist);
  MyListPrivate *priv = my_list_get_instance_private (mylist);
  GtkAdjustment *adj;
  gdouble width;

  adj = priv->hadjustment;

  width = gtk_widget_get_allocated_width (widget);
  g_object_set (adj,
                "lower", 0.0,
                "upper", width,
                "page-size", width,
                "step-increment", width * 0.1,
                "page-increment", width * 0.9,
                NULL);

  gtk_adjustment_set_value (adj, 0.0);
}

static void
set_vadjustment_values (MyList *mylist)
{
  GtkWidget *widget = GTK_WIDGET (mylist);
  MyListPrivate *priv = my_list_get_instance_private (mylist);
  GtkAdjustment *adj;
  gdouble height;
  gdouble old_value;
  gdouble new_value;
  gdouble upper;

  adj = priv->vadjustment;
  old_value = gtk_adjustment_get_value (adj);
  height = gtk_widget_get_allocated_height (widget);
  upper = MAX (height, priv->height);
 
  g_object_set (adj,
                "lower", 0.0,
                "upper", upper,
                "page-size", height,
                "step-increment", height * 0.1,
                "page-increment", height * 0.9,
                NULL);

  new_value = CLAMP (old_value, 0.0, upper - height);
  if (new_value != old_value)
    gtk_adjustment_set_value (adj, new_value);
}

static gboolean
update_adjustments (gpointer data)
{
  set_hadjustment_values (MY_LIST (data));
  set_vadjustment_values (MY_LIST (data));

  return G_SOURCE_REMOVE;
}

static void
size_allocate_children_up (GtkWidget *widget, GtkAllocation *allocation)
{
  MyListPrivate *priv = my_list_get_instance_private (MY_LIST (widget));
  GtkAllocation child_allocation;
  GList *l;
  gint m;
  gdouble position;
  gdouble row_height;
  gdouble offset;
  gint n_children;
  gint model_size;
  gint n;

  model_size = my_model_get_size (priv->model);

  offset = gtk_adjustment_get_upper (priv->vadjustment) - gtk_adjustment_get_page_size (priv->vadjustment) - gtk_adjustment_get_value (priv->vadjustment);

  n = floor (offset / priv->average_row_height);
  position = offset - n * priv->average_row_height;
  m = model_size - 1 - n;

  child_allocation.x = allocation->x - gtk_adjustment_get_value (priv->hadjustment);
  child_allocation.width = allocation->width;
  child_allocation.y = allocation->y + allocation->height + position;

  row_height = 0.0;
  n_children = 0;
  l = priv->children;

  for (; m >= 0; m--)
    {
      GtkWidget *child;
      gint min, nat;

      if (l == NULL)
        {
          child = priv->create_row ();
          gtk_widget_show (child);
          gtk_widget_set_parent (child, widget);
          priv->children = g_list_append (priv->children, child);
        }
      else
        {
          child = l->data;
          l = l->next;
        }

      priv->connect_row (priv->model, m, child);

      gtk_widget_get_preferred_height_for_width (child,
                                                 allocation->width,
                                                 &min, &nat); 

      child_allocation.height = nat;
      child_allocation.y -= child_allocation.height;
      gtk_widget_size_allocate (child, &child_allocation);

      row_height += child_allocation.height;
      n_children += 1;

      position -= child_allocation.height;
      if (position <= - allocation->height)
        break;
    }

  priv->height = (model_size - n_children) * priv->average_row_height + row_height;

  priv->average_row_height = priv->height / model_size;

  if (l)
    {
      if (l->prev)
        l->prev->next = NULL;
      g_list_free_full (l, (GDestroyNotify)gtk_widget_unparent);
    }
}
                  
static void
size_allocate_children_down (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  MyListPrivate *priv = my_list_get_instance_private (MY_LIST (widget));
  GtkAllocation child_allocation;
  GList *l;
  gint m;
  gdouble position;
  gdouble row_height;
  gdouble offset;
  gint n_children;
  gint model_size;
  gint n;

  model_size = my_model_get_size (priv->model);

  offset = gtk_adjustment_get_value (priv->vadjustment);

  n = floor (offset / priv->average_row_height);
  position = n * priv->average_row_height - offset;
  m = n;

  child_allocation.x = allocation->x - gtk_adjustment_get_value (priv->hadjustment);
  child_allocation.width = allocation->width;

  child_allocation.y = allocation->y + position;

  row_height = 0.0;
  n_children = 0;
  l = priv->children;

  for (; m < model_size; m++)
    {
      GtkWidget *child;
      gint min, nat;

      if (l == NULL)
        {
          child = priv->create_row ();
          gtk_widget_show (child);
          gtk_widget_set_parent (child, widget);
          priv->children = g_list_append (priv->children, child);
        }
      else
        {
          child = l->data;
          l = l->next;
        }

      priv->connect_row (priv->model, m, child);

      gtk_widget_get_preferred_height_for_width (child,
                                                 allocation->width,
                                                 &min, &nat); 

      child_allocation.height = nat;
      gtk_widget_size_allocate (child, &child_allocation);
      child_allocation.y += child_allocation.height;

      row_height += child_allocation.height;
      n_children += 1;

      position += child_allocation.height;
      if (position >= allocation->height)
        break;
    }

  priv->height = (model_size - n_children) * priv->average_row_height + row_height;

  priv->average_row_height = priv->height / model_size;

  if (l)
    {
      if (l->prev)
        l->prev->next = NULL;
      g_list_free_full (l, (GDestroyNotify)gtk_widget_unparent);
    }
}

static void
my_list_size_allocate (GtkWidget     *widget,
                       GtkAllocation *allocation)
{
  MyListPrivate *priv = my_list_get_instance_private (MY_LIST (widget));

  gtk_widget_set_allocation (widget, allocation);

  if (2 * gtk_adjustment_get_value (priv->vadjustment) >
      gtk_adjustment_get_upper (priv->vadjustment) - gtk_adjustment_get_page_size (priv->vadjustment))
    size_allocate_children_up (widget, allocation);
  else
    size_allocate_children_down (widget, allocation);

  g_idle_add (update_adjustments, widget);
}

static void
adjustment_changed (GtkAdjustment *adjustment,
                    MyList        *mylist)
{
  gtk_widget_queue_resize (GTK_WIDGET (mylist));
}

static void
set_hadjustment (MyList        *mylist,
                 GtkAdjustment *adjustment)
{
  MyListPrivate *priv = my_list_get_instance_private (mylist);

  if (adjustment && priv->hadjustment == adjustment)
    return;

  if (priv->hadjustment)
    {
      g_signal_handlers_disconnect_by_func (priv->hadjustment,
                                            adjustment_changed, mylist);
      g_object_unref (priv->hadjustment);
    }

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (adjustment_changed), mylist);

  priv->hadjustment = g_object_ref_sink (adjustment);
  set_hadjustment_values (mylist);
}

static void
set_vadjustment (MyList        *mylist,
                 GtkAdjustment *adjustment)
{
  MyListPrivate *priv = my_list_get_instance_private (mylist);

  if (adjustment && priv->vadjustment == adjustment)
    return;

  if (priv->vadjustment)
    {
      g_signal_handlers_disconnect_by_func (priv->vadjustment,
                                            adjustment_changed, mylist);
      g_object_unref (priv->vadjustment);
    }

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (adjustment_changed), mylist);

  priv->vadjustment = g_object_ref_sink (adjustment);
  set_vadjustment_values (mylist);
}

static void
my_list_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  MyListPrivate *priv = my_list_get_instance_private (MY_LIST (object));

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, priv->hadjustment);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, priv->vadjustment);
      break;
    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, GTK_SCROLL_MINIMUM);
      break;
    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, GTK_SCROLL_MINIMUM);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
my_list_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  MyList *mylist = MY_LIST (object);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      set_hadjustment (mylist, g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      set_vadjustment (mylist, g_value_get_object (value));
      break;
    case PROP_HSCROLL_POLICY:
      break;
    case PROP_VSCROLL_POLICY:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
my_list_class_init (MyListClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->finalize = my_list_finalize;
  object_class->get_property = my_list_get_property;
  object_class->set_property = my_list_set_property;
  widget_class->get_request_mode = my_list_get_request_mode;
  widget_class->get_preferred_width = my_list_get_preferred_width;
  widget_class->get_preferred_width_for_height = my_list_get_preferred_width_for_height;
  widget_class->get_preferred_height = my_list_get_preferred_height;
  widget_class->get_preferred_height_for_width = my_list_get_preferred_height_for_width;
  widget_class->size_allocate = my_list_size_allocate;

  container_class->forall = my_list_forall;

  g_object_class_override_property (object_class, PROP_HADJUSTMENT, "hadjustment"); 
  g_object_class_override_property (object_class, PROP_VADJUSTMENT, "vadjustment"); 
  g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy"); 
  g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy"); 
}

MyList *
my_list_new (void)
{
  return MY_LIST (g_object_new (MY_TYPE_LIST, NULL));
}

void
my_list_set_model (MyList         *list,
                   MyModel        *model,
                   CreateRowFunc   create_row,
                   ConnectRowFunc  connect_row)
{
  MyListPrivate *priv = my_list_get_instance_private (list);

  if (priv->model)
    g_object_unref (priv->model);
  priv->model = model;
  if (priv->model)
    g_object_ref (priv->model);

  priv->create_row = create_row;
  priv->connect_row = connect_row;
}
