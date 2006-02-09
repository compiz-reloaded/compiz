#ifndef COMPIZ_WINDOW_MANAGER_H
#define COMPIZ_WINDOW_MANAGER_H

#include <glib-object.h>

#include "gnome-window-manager.h"

#define COMPIZ_WINDOW_MANAGER(obj)					\
    G_TYPE_CHECK_INSTANCE_CAST (obj, compiz_window_manager_get_type (), \
				CompizWindowManager)

#define COMPIZ_WINDOW_MANAGER_CLASS(klass)			       \
    G_TYPE_CHECK_CLASS_CAST (klass, compiz_window_manager_get_type (), \
			     MetacityWindowManagerClass)

#define IS_COMPIZ_WINDOW_MANAGER(obj)					\
    G_TYPE_CHECK_INSTANCE_TYPE (obj, compiz_window_manager_get_type ())


typedef struct _CompizWindowManager	   CompizWindowManager;
typedef struct _CompizWindowManagerClass   CompizWindowManagerClass;
typedef struct _CompizWindowManagerPrivate CompizWindowManagerPrivate;

struct _CompizWindowManager {
    GnomeWindowManager	       parent;
    CompizWindowManagerPrivate *p;
};

struct _CompizWindowManagerClass {
    GnomeWindowManagerClass klass;
};

GType
compiz_window_manager_get_type (void);

GObject *
window_manager_new (int expected_interface_version);

#endif
