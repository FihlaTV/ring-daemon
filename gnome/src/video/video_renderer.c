/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "video_renderer.h"
#include "shm_header.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#include "logger.h"
#include "unused.h"

/* This macro will implement the video_renderer_get_type function
   and define a parent class pointer accessible from the whole .c file */
G_DEFINE_TYPE(VideoRenderer, video_renderer, G_TYPE_OBJECT);

#define VIDEO_RENDERER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            VIDEO_RENDERER_TYPE, VideoRendererPrivate))

enum
{
    PROP_0,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_DRAWAREA,
    PROP_SHM_PATH,
    PROP_LAST
};

static GParamSpec *properties[PROP_LAST] = { NULL, };

static void video_renderer_finalize(GObject *gobject);
static void video_renderer_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void video_renderer_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

/* Our private member structure */
struct _VideoRendererPrivate {
    guint width;
    guint height;
    gchar *shm_path;

    ClutterActor *texture;

    gpointer drawarea;
    gint fd;
    SHMHeader *shm_area;
    gsize shm_area_len;
    guint buffer_gen;
};

static void
video_renderer_class_init(VideoRendererClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(VideoRendererPrivate));
    gobject_class->finalize = video_renderer_finalize;
    gobject_class->get_property = video_renderer_get_property;
    gobject_class->set_property = video_renderer_set_property;

    properties[PROP_DRAWAREA] = g_param_spec_pointer("drawarea", "DrawArea",
                                                    "Pointer to the drawing area",
                                                    G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_WIDTH] = g_param_spec_int("width", "Width", "Width of video", G_MININT, G_MAXINT, 0,
                                              G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_HEIGHT] = g_param_spec_int("height", "Height", "Height of video", G_MININT, G_MAXINT, 0,
                                               G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_SHM_PATH] = g_param_spec_string("shm-path", "ShmPath", "Unique path for shared memory", "",
                                                    G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(gobject_class,
            PROP_LAST,
            properties);
}

static void
video_renderer_get_property(GObject *object, guint prop_id,
                            GValue *value, GParamSpec *pspec)
{
    VideoRenderer *renderer = VIDEO_RENDERER(object);
    VideoRendererPrivate *priv = renderer->priv;

    switch (prop_id) {
        case PROP_DRAWAREA:
            g_value_set_pointer(value, priv->drawarea);
            break;
        case PROP_WIDTH:
            g_value_set_int(value, priv->width);
            break;
        case PROP_HEIGHT:
            g_value_set_int(value, priv->height);
            break;
        case PROP_SHM_PATH:
            g_value_set_string(value, priv->shm_path);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
video_renderer_set_property (GObject *object, guint prop_id,
                            const GValue *value, GParamSpec *pspec)
{
    VideoRenderer *renderer = VIDEO_RENDERER(object);
    VideoRendererPrivate *priv = renderer->priv;

    switch (prop_id) {
        case PROP_DRAWAREA:
            priv->drawarea = g_value_get_pointer(value);
            break;
        case PROP_WIDTH:
            priv->width = g_value_get_int(value);
            break;
        case PROP_HEIGHT:
            priv->height = g_value_get_int(value);
            break;
        case PROP_SHM_PATH:
            g_free(priv->shm_path);
            priv->shm_path = g_value_dup_string(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


static void
video_renderer_init(VideoRenderer *self)
{
    VideoRendererPrivate *priv;
    self->priv = priv = VIDEO_RENDERER_GET_PRIVATE(self);
    priv->width = 0;
    priv->height = 0;
    priv->shm_path = NULL;
    priv->texture = NULL;
    priv->drawarea = NULL;
    priv->fd = -1;
    priv->shm_area = MAP_FAILED;
    priv->shm_area_len = 0;
    priv->buffer_gen = 0;
}

static void
video_renderer_stop_shm(VideoRenderer *self)
{
    g_return_if_fail(IS_VIDEO_RENDERER(self));
    VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(self);
    if (priv->fd >= 0)
        close(priv->fd);
    priv->fd = -1;

    if (priv->shm_area != MAP_FAILED)
        munmap(priv->shm_area, priv->shm_area_len);
    priv->shm_area_len = 0;
    priv->shm_area = MAP_FAILED;
}


static void
video_renderer_finalize(GObject *obj)
{
    VideoRenderer *self = VIDEO_RENDERER(obj);
    video_renderer_stop_shm(self);

    /* Chain up to the parent class */
    G_OBJECT_CLASS(video_renderer_parent_class)->finalize(obj);
}

static gboolean
video_renderer_start_shm(VideoRenderer *self)
{
    /* First test that 'self' is of the correct type */
    g_return_val_if_fail(IS_VIDEO_RENDERER(self), FALSE);
    VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(self);
    if (priv->fd != -1) {
        ERROR("fd must be -1");
        return FALSE;
    }

    priv->fd = shm_open(priv->shm_path, O_RDWR, 0);
    if (priv->fd < 0) {
        DEBUG("could not open shm area \"%s\", shm_open failed:%s", priv->shm_path, strerror(errno));
        return FALSE;
    }
    priv->shm_area_len = sizeof(SHMHeader);
    priv->shm_area = mmap(NULL, priv->shm_area_len, PROT_READ | PROT_WRITE, MAP_SHARED, priv->fd, 0);
    if (priv->shm_area == MAP_FAILED) {
        DEBUG("Could not map shm area, mmap failed");
        return FALSE;
    }
    return TRUE;
}

static const gint TIMEOUT_SEC = 1; // 1 second

static struct timespec
create_timeout()
{
    struct timespec timeout = {0, 0};
    if (clock_gettime(CLOCK_REALTIME, &timeout) == -1)
        perror("clock_gettime");
    timeout.tv_sec += TIMEOUT_SEC;
    return timeout;
}


static gboolean
shm_lock(SHMHeader *shm_area)
{
    const struct timespec timeout = create_timeout();
    /* We need an upper limit on how long we'll wait to avoid locking the whole GUI */
    if (sem_timedwait(&shm_area->mutex, &timeout) == ETIMEDOUT) {
        ERROR("Timed out before shm lock was acquired");
        return FALSE;
    }
    return TRUE;
}

static void
shm_unlock(SHMHeader *shm_area)
{
    sem_post(&shm_area->mutex);
}

static gboolean
video_renderer_resize_shm(VideoRendererPrivate *priv)
{
    while ((sizeof(SHMHeader) + priv->shm_area->buffer_size) > priv->shm_area_len) {
        const size_t new_size = sizeof(SHMHeader) + priv->shm_area->buffer_size;

        shm_unlock(priv->shm_area);
        if (munmap(priv->shm_area, priv->shm_area_len)) {
            DEBUG("Could not unmap shared area:%s", strerror(errno));
            return FALSE;
        }

        priv->shm_area = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, priv->fd, 0);
        priv->shm_area_len = new_size;

        if (!priv->shm_area) {
            priv->shm_area = 0;
            DEBUG("Could not remap shared area");
            return FALSE;
        }

        priv->shm_area_len = new_size;
        if (!shm_lock(priv->shm_area))
            return FALSE;
    }
    return TRUE;
}

static void
video_renderer_render_to_texture(VideoRendererPrivate *priv)
{
    if (!shm_lock(priv->shm_area))
        return;

    // wait for a new buffer
    while (priv->buffer_gen == priv->shm_area->buffer_gen) {
        shm_unlock(priv->shm_area);
        const struct timespec timeout = create_timeout();
        // Could not decrement semaphore in time, returning
        if (sem_timedwait(&priv->shm_area->notification, &timeout) < 0)
            return;

        if (!shm_lock(priv->shm_area))
            return;
    }

    if (!video_renderer_resize_shm(priv)) {
        ERROR("Could not resize shared memory");
        return;
    }

    const gint BPP = 4;
    const gint ROW_STRIDE = BPP * priv->width;
    /* update the clutter texture */
    clutter_texture_set_from_rgb_data(CLUTTER_TEXTURE(priv->texture),
            (guchar*) priv->shm_area->data,
            TRUE,
            priv->width,
            priv->height,
            ROW_STRIDE,
            BPP,
            CLUTTER_TEXTURE_RGB_FLAG_BGR,
            NULL);
    priv->buffer_gen = priv->shm_area->buffer_gen;
    shm_unlock(priv->shm_area);
}


static gboolean
render_frame_from_shm(VideoRendererPrivate *priv)
{
    if (!GTK_IS_WIDGET(priv->drawarea))
        return FALSE;
    GtkWidget *parent = gtk_widget_get_parent(priv->drawarea);
    if (!parent || !CLUTTER_IS_ACTOR(priv->texture))
        return FALSE;
    const gint parent_width = gtk_widget_get_allocated_width(parent);
    const gint parent_height = gtk_widget_get_allocated_height(parent);
    clutter_actor_set_size(priv->texture, parent_width, parent_height);
    video_renderer_render_to_texture(priv);

    return TRUE;
}

void video_renderer_stop(VideoRenderer *renderer)
{
    VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(renderer);
    if (priv && priv->drawarea && GTK_IS_WIDGET(priv->drawarea))
        gtk_widget_hide(GTK_WIDGET(priv->drawarea));

    g_object_unref(G_OBJECT(renderer));
}

static gboolean
update_texture(gpointer data)
{
    VideoRenderer *renderer = (VideoRenderer *) data;
    VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(renderer);

    const gboolean ret = render_frame_from_shm(priv);

    if (!ret) {
        video_renderer_stop(data);
        g_object_unref(G_OBJECT(data));
    }

    return ret;
}

/**
 * video_renderer_new:
 *
 * Create a new #VideoRenderer instance.
 */
VideoRenderer *
video_renderer_new(GtkWidget *drawarea, gint width, gint height, gchar *shm_path)
{
    VideoRenderer *rend = g_object_new(VIDEO_RENDERER_TYPE, "drawarea", (gpointer) drawarea,
            "width", width, "height", height, "shm-path", shm_path, NULL);
    if (!video_renderer_start_shm(rend)) {
        ERROR("Could not start SHM");
        return NULL;
    }
    return rend;
}

gboolean
video_renderer_run(VideoRenderer *self)
{
    g_return_val_if_fail(IS_VIDEO_RENDERER(self), FALSE);
    VideoRendererPrivate * priv = VIDEO_RENDERER_GET_PRIVATE(self);
    g_return_val_if_fail(priv->fd > 0, FALSE);

    GtkWindow *win = GTK_WINDOW(gtk_widget_get_toplevel(priv->drawarea));
    GdkGeometry geom = {
        .min_aspect = (gdouble) priv->width / priv->height,
        .max_aspect = (gdouble) priv->width / priv->height,
    };
    gtk_window_set_geometry_hints(win, NULL, &geom, GDK_HINT_ASPECT);

    if (!GTK_CLUTTER_IS_EMBED(priv->drawarea))
        ERROR("Drawing area is not a GtkClutterEmbed widget");

    ClutterActor *stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(priv->drawarea));
    g_assert(stage);
    priv->texture = clutter_texture_new();

    /* Add ClutterTexture to the stage */
    clutter_container_add(CLUTTER_CONTAINER(stage), priv->texture, NULL);
    clutter_actor_show_all(stage);

    /* frames are read and saved here */
    g_object_ref(self);
    const gint FRAME_INTERVAL = 30; // ms
    g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, FRAME_INTERVAL, update_texture, self, NULL);

    gtk_widget_show_all(GTK_WIDGET(priv->drawarea));

    return TRUE;
}