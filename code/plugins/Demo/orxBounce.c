/* Orx - Portable Game Engine
 *
 * Orx is the legal property of its developers, whose names
 * are listed in the COPYRIGHT file distributed
 * with this source distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file orxBounce.c
 * @date 07/04/2008
 * @author iarwain@orx-project.org
 *
 * Bounce demo
 *
 * @todo
 */


#include "orx.h"

orxSTATIC orxU32 su32BallCounter = 0;

/** Bounce event handler
 * @param[in]   _pstEvent                     Sent event
 * @return      orxSTATUS_SUCCESS if handled / orxSTATUS_FAILURE otherwise
 */
orxSTATIC orxFASTCALL orxSTATUS orxBounce_EventHandler(orxCONST orxEVENT *_pstEvent)
{
  orxSTATUS eResult = orxSTATUS_FAILURE;

  /* Checks */
  orxASSERT(_pstEvent->eType == orxEVENT_TYPE_PHYSICS);

  /* Going out of world? */
  if(_pstEvent->eID == orxPHYSICS_EVENT_OUT_OF_WORLD)
  {
    /* Deletes corresponding object */
    orxObject_Delete(orxOBJECT(_pstEvent->hSender));

    /* Updates ball counter */
    su32BallCounter--;
  }
  /* Colliding? */
  else if(_pstEvent->eID == orxPHYSICS_EVENT_CONTACT_ADD)
  {
    /* Adds bump FX on both objects */
    orxObject_AddFX(orxOBJECT(_pstEvent->hSender), "Bump");
    orxObject_AddFX(orxOBJECT(_pstEvent->hRecipient), "Bump");
  }

  /* Done! */
  return eResult;
}

/** Update callback
 */
orxVOID orxFASTCALL orxBounce_Update(orxCONST orxCLOCK_INFO *_pstClockInfo, orxVOID *_pstContext)
{
  /* Selects config section */
  orxConfig_SelectSection("Bounce");

  /* Clicking? */
  if((orxMouse_IsButtonPressed(orxMOUSE_BUTTON_RIGHT)) || (orxMouse_IsButtonPressed(orxMOUSE_BUTTON_LEFT)))
  {
    orxVECTOR   vScreenPos, vWorldPos;
    orxOBJECT  *pstObject;

    /* Gets mouse coordinates */
    orxMouse_GetPosition(&vScreenPos);

    /* Has a matching world position? */
    if(orxRender_GetWorldPosition(&vScreenPos, &vWorldPos) != orxNULL)
    {
      /* Left clicking */
      if(orxMouse_IsButtonPressed(orxMOUSE_BUTTON_LEFT))
      {
        /* Under limit? */
        if(su32BallCounter < orxConfig_GetU32("BallLimit"))
        {
          /* Updates position */
          vWorldPos.fZ += orxFLOAT_1;

          /* Spawns a ball under the cursor */
          pstObject = orxObject_CreateFromConfig("Ball");
          orxObject_SetPosition(pstObject, &vWorldPos);

          /* Update counter */
          su32BallCounter++;
        }
      }
      else
      {
        /* Picks object under mouse */
        pstObject = orxObject_Pick(&vWorldPos);

        /* Found and is a ball? */
        if((pstObject) && (!orxString_Compare(orxObject_GetName(pstObject), "Ball")))
        {
          /* Adds FX */
          orxObject_AddFX(pstObject, "Pick");
        }
      }
    }
  }
}

/** Inits the bounce demo
 */
orxSTATIC orxSTATUS orxBounce_Init()
{
  orxCLOCK *pstClock;
  orxSTATUS eResult;

  /* Loads config file and selects its section */
  orxConfig_Load("Bounce.ini");
  orxConfig_SelectSection("Bounce");

  /* Creates walls */
  orxObject_CreateFromConfig("Wall1");
  orxObject_CreateFromConfig("Wall2");
  orxObject_CreateFromConfig("Wall3");
  orxObject_CreateFromConfig("Wall4");

  /* Creates viewport on screen */
  orxViewport_CreateFromConfig("BounceViewport");

  /* Gets rendering clock */
  pstClock = orxClock_FindFirst(orx2F(-1.0f), orxCLOCK_TYPE_RENDER);

  /* Registers callback */
  eResult = orxClock_Register(pstClock, &orxBounce_Update, orxNULL, orxMODULE_ID_MAIN, orxCLOCK_FUNCTION_PRIORITY_NORMAL);

  /* Registers event handler */
  eResult = ((eResult != orxSTATUS_FAILURE) && (orxEvent_AddHandler(orxEVENT_TYPE_PHYSICS, orxBounce_EventHandler) != orxSTATUS_FAILURE)) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;

  /* Done! */
  return eResult;
}

/* Registers plugin entry */
orxPLUGIN_DECLARE_ENTRY_POINT(orxBounce_Init);
