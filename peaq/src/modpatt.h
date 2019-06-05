/* GstPEAQ
 * Copyright (C) 2006, 2012, 2013, 2015
 * Martin Holters <martin.holters@hsu-hh.de>
 *
 * modpatt.h: Modulation pattern processor.
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


#ifndef __MODPATT_H__
#define __MODPATT_H__ 1

#include "earmodel.h"
#include <glib-object.h>

#define PEAQ_TYPE_MODULATIONPROCESSOR (peaq_modulationprocessor_get_type ())
#define PEAQ_MODULATIONPROCESSOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST (obj, PEAQ_TYPE_MODULATIONPROCESSOR, \
			       PeaqModulationProcessor))
#define PEAQ_MODULATIONPROCESSOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST (klass, PEAQ_TYPE_MODULATIONPROCESSOR, \
			    PeaqModulationProcessorClass))
#define PEAQ_IS_MODULATIONPROCESSOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE (obj, PEAQ_TYPE_MODULATIONPROCESSOR))
#define PEAQ_IS_MODULATIONPROCESSOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE (klass, PEAQ_TYPE_MODULATIONPROCESSOR))
#define PEAQ_MODULATIONPROCESSOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS (obj, PEAQ_TYPE_MODULATIONPROCESSOR, \
			      PeaqModulationProcessorClass))

typedef struct _PeaqModulationProcessorClass PeaqModulationProcessorClass;
typedef struct _PeaqModulationProcessor PeaqModulationProcessor;

GType peaq_modulationprocessor_get_type ();
PeaqModulationProcessor *peaq_modulationprocessor_new (PeaqEarModel *ear_model);
void peaq_modulationprocessor_set_ear_model (PeaqModulationProcessor *modproc,
                                             PeaqEarModel *ear_model);
PeaqEarModel *peaq_modulationprocessor_get_ear_model (PeaqModulationProcessor const *modproc);
void peaq_modulationprocessor_process (PeaqModulationProcessor *modproc,
				       gdouble const* unsmeared_excitation);
gdouble const *peaq_modulationprocessor_get_average_loudness (PeaqModulationProcessor const *modproc);
gdouble const *peaq_modulationprocessor_get_modulation (PeaqModulationProcessor const *modproc);
#endif
