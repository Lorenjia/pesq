/* GstPEAQ
 * Copyright (C) 2006, 2011, 2012, 2013, 2014, 2015
 * Martin Holters <martin.holters@hsu-hh.de>
 *
 * gstpeaq.h: Compute objective audio quality measures
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __GST_PEAQ_H__
#define __GST_PEAQ_H__

#include <glib-object.h>

G_BEGIN_DECLS;

#define GST_TYPE_PEAQ            (gst_peaq_get_type())
#define GST_PEAQ(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
							     GST_TYPE_PEAQ, \
							     GstPeaq))
#define GST_PEAQ_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), \
							  GST_TYPE_PEAQ, \
							  GstPeaqClass))
#define GST_IS_PEAQ(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
							     GST_TYPE_PEAQ))
#define GST_IS_PEAQ_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
							  GST_TYPE_PEAQ))
#define GST_PEAQ_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS(obj, \
							    GST_TYPE_PEAQ, \
							    GstPeaqClass))

typedef struct _GstPeaq GstPeaq;
typedef struct _GstPeaqClass GstPeaqClass;

GType gst_peaq_get_type ();

G_END_DECLS;

#endif /* __GST_PEAQ_H__ */
