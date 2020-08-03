/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bgpencil
 */

#include <stdio.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "../gpencil_io_exporter.h"
#include "gpencil_io_svg.h"

using blender::io::gpencil::GpencilExporterSVG;

static bool is_keyframe_empty(bGPdata *gpd, int framenum)
{
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      if (gpf->framenum == framenum) {
        return false;
      }
    }
  }
  return true;
}

/* Export current frame. */
static bool gpencil_io_export_frame(GpencilExporterSVG *writter,
                                    const GpencilExportParams *iparams,
                                    float frame_offset[2],
                                    const bool newpage,
                                    const bool savepage)
{

  bool result = false;
  switch (iparams->mode) {
    case GP_EXPORT_TO_SVG: {
      writter->set_frame_number(iparams->framenum);
      writter->set_frame_offset(frame_offset);
      result = writter->write(std::string(""), newpage, savepage);
      break;
    }
    default:
      break;
  }

  return result;
}

/* Main export entry point function. */
bool gpencil_io_export(GpencilExportParams *iparams)
{
  Main *bmain = CTX_data_main(iparams->C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(iparams->C);
  Scene *scene = CTX_data_scene(iparams->C);

  Object *ob = CTX_data_active_object(iparams->C);
  Object *ob_eval_ = (Object *)DEG_get_evaluated_id(depsgraph, &ob->id);
  bGPdata *gpd_eval = (bGPdata *)ob_eval_->data;

  const bool only_active_frame = ((iparams->flag & GP_EXPORT_ACTIVE_FRAME) != 0);

  int oldframe = (int)DEG_get_ctime(depsgraph);
  bool done = false;

  /* Calc paper sizes. */
  const float blocks[2] = {3.0f, 1.0f};

  /* Prepare document. */
  // TODO: Fix paper using parameter
  copy_v2_v2(iparams->paper_size, paper_size[0]);

  GpencilExporterSVG writter = GpencilExporterSVG(iparams);

  float frame_box[2] = {iparams->paper_size[0] / (blocks[0] + 1.0f),
                        iparams->paper_size[1] / (blocks[0] + 1.0f)};
  writter.set_frame_box(frame_box);

  const float gap[2] = {frame_box[0] / (blocks[0] + 1.0f), frame_box[1] / (blocks[1] + 1.0f)};

  float frame_offset[2] = {gap[0], gap[1]};

  if (only_active_frame) {
    float no_offset[2] = {0.0f, 0.0f};
    float ratio[2] = {1.0f, 1.0f};
    writter.set_frame_ratio(ratio);
    done |= gpencil_io_export_frame(&writter, iparams, no_offset, true, true);
  }
  else {
    bool newpage = true;
    bool savepage = false;
    float render_ratio[2];
    render_ratio[0] = frame_box[0] / ((scene->r.xsch * scene->r.size) / 100);
    render_ratio[1] = frame_box[1] / ((scene->r.ysch * scene->r.size) / 100);
    writter.set_frame_ratio(render_ratio);

    int x = 1;
    for (int i = iparams->frame_start; i < iparams->frame_end + 1; i++) {
      if (is_keyframe_empty(gpd_eval, i)) {
        continue;
      }
      if (x == 3) {
        savepage = true;
      }

      CFRA = i;
      BKE_scene_graph_update_for_newframe(depsgraph, bmain);
      sprintf(iparams->file_subfix, "%04d", i);
      iparams->framenum = i;
      done |= gpencil_io_export_frame(&writter, iparams, frame_offset, newpage, savepage);

      if (x == 3) {
        savepage = true;
        newpage = false;
        x = 0;
      }
      else {
        newpage = false;
        x++;
      }

      frame_offset[0] += frame_box[0];
      frame_offset[0] += gap[0];
    }
  }

  /* Return frame state and DB to original state. */
  if (!only_active_frame) {
    CFRA = oldframe;
    BKE_scene_graph_update_for_newframe(depsgraph, bmain);
  }

  return done;
}
