#include <gtk/gtk.h>
#include "list.h"
#include "model.h"

static GtkWidget *
create_label (void)
{
  return gtk_label_new ("");
}

static void
connect_label (MyModel   *model,
               gint       n,
               GtkWidget *widget)
{
  const gchar *item;

  item = my_model_get_item (model, n);
  gtk_label_set_label (GTK_LABEL (widget), item);
}

static GtkWidget *
create_entry (void)
{
  return gtk_entry_new ();
}

static void
connect_entry (MyModel   *model,
               gint       n,
               GtkWidget *widget)
{
  const gchar *item;

  item = my_model_get_item (model, n);
  gtk_entry_set_text (GTK_ENTRY (widget), item);
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *sw;
  MyList *list;
  MyModel *model;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (window), sw);

  list = my_list_new ();
  model = my_model_new (50000);
  my_list_set_model (list, model, create_label, connect_label);
  //my_list_set_model (list, model, create_entry, connect_entry);

  gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (list));

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}

