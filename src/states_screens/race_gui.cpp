//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004-2015 Steve Baker <sjbaker1@airmail.net>
//  Copyright (C) 2006-2015 Joerg Henrichs, SuperTuxKart-Team, Steve Baker
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "states_screens/race_gui.hpp"

using namespace irr;

#include <algorithm>
#include <limits>

#include "challenges/story_mode_timer.hpp"
#include "challenges/unlock_manager.hpp"
#include "config/user_config.hpp"
#include "font/font_drawer.hpp"
#include "graphics/camera.hpp"
#include "graphics/2dutils.hpp"
#ifndef SERVER_ONLY
#include "graphics/glwrap.hpp"
#endif
#include "graphics/irr_driver.hpp"
#include "graphics/material.hpp"
#include "graphics/material_manager.hpp"
#include "guiengine/engine.hpp"
#include "guiengine/modaldialog.hpp"
#include "guiengine/scalable_font.hpp"
#include "io/file_manager.hpp"
#include "items/powerup_manager.hpp"
#include "items/powerup.hpp"
#include "items/attachment_manager.hpp"
#include "items/item_manager.hpp"
#include "items/projectile_manager.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/controller/controller.hpp"
#include "karts/controller/spare_tire_ai.hpp"
#include "karts/kart_properties.hpp"
#include "karts/kart_properties_manager.hpp"
#include "karts/max_speed.hpp"
#include "modes/capture_the_flag.hpp"
#include "modes/follow_the_leader.hpp"
#include "modes/linear_world.hpp"
#include "modes/world.hpp"
#include "modes/soccer_world.hpp"
#include "network/protocols/client_lobby.hpp"
#include "race/race_manager.hpp"
#include "states_screens/race_gui_multitouch.hpp"
#include "tracks/check_line.hpp"
#include "tracks/check_manager.hpp"
#include "tracks/check_structure.hpp"
#include "tracks/track.hpp"
#include "tracks/track_object_manager.hpp"
#include "utils/constants.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"
#include "karts/skidding.hpp"

/** The constructor is called before anything is attached to the scene node.
 *  So rendering to a texture can be done here. But world is not yet fully
 *  created, so only the race manager can be accessed safely.
 */
RaceGUI::RaceGUI()
{
    m_enabled = true;
    
    if (UserConfigParams::m_artist_debug_mode && UserConfigParams::m_hide_gui)
        m_enabled = false;

    initSize();
    bool multitouch_enabled = (UserConfigParams::m_multitouch_active == 1 && 
                               irr_driver->getDevice()->supportsTouchDevice()) ||
                               UserConfigParams::m_multitouch_active > 1;
    
    if (multitouch_enabled && UserConfigParams::m_multitouch_draw_gui &&
        RaceManager::get()->getNumLocalPlayers() == 1)
    {
        m_multitouch_gui = new RaceGUIMultitouch(this);
    }
    
    calculateMinimapSize();

    m_is_tutorial = (RaceManager::get()->getTrackName() == "tutorial");

    // Load speedmeter texture before rendering the first frame
    m_speed_meter_icon = material_manager->getMaterial("speedback.png");
    m_speed_meter_icon->getTexture(false,false);
    m_speed_bar_icon   = material_manager->getMaterial("speedfore.png");
    m_speed_bar_icon->getTexture(false,false);
    //createMarkerTexture();

    // Load icon textures for later reuse
    m_red_team = irr_driver->getTexture(FileManager::GUI_ICON, "soccer_ball_red.png");
    m_blue_team = irr_driver->getTexture(FileManager::GUI_ICON, "soccer_ball_blue.png");
    m_red_flag = irr_driver->getTexture(FileManager::GUI_ICON, "red_flag.png");
    m_blue_flag = irr_driver->getTexture(FileManager::GUI_ICON, "blue_flag.png");
    m_soccer_ball = irr_driver->getTexture(FileManager::GUI_ICON, "soccer_ball_normal.png");
    m_heart_icon = irr_driver->getTexture(FileManager::GUI_ICON, "heart.png");
    m_basket_ball_icon = irr_driver->getTexture(FileManager::GUI_ICON, "rubber_ball-icon.png");
    m_checkline_icon = irr_driver->getTexture(FileManager::GUI_ICON, "checkline.png");
    m_champion = irr_driver->getTexture(FileManager::GUI_ICON, "cup_gold.png");
}   // RaceGUI

// ----------------------------------------------------------------------------
/** Called when loading the race gui or screen resized. */
void RaceGUI::initSize()
{
    RaceGUIBase::initSize();
    // Determine maximum length of the rank/lap text, in order to
    // align those texts properly on the right side of the viewport.
    gui::ScalableFont* font = GUIEngine::getHighresDigitFont();
    core::dimension2du area = font->getDimension(L"99:99.999");
    m_timer_width = area.Width;
    m_font_height = area.Height;

    area = font->getDimension(L"99.999");
    m_small_precise_timer_width = area.Width;

    area = font->getDimension(L"99:99.999");
    m_big_precise_timer_width = area.Width;

    area = font->getDimension(L"-");
    m_negative_timer_additional_width = area.Width;

    if (RaceManager::get()->getMinorMode()==RaceManager::MINOR_MODE_FOLLOW_LEADER ||
        RaceManager::get()->isBattleMode()     ||
        RaceManager::get()->getNumLaps() > 9)
        m_lap_width = font->getDimension(L"99/99").Width;
    else
        m_lap_width = font->getDimension(L"9/9").Width;
}   // initSize

//-----------------------------------------------------------------------------
RaceGUI::~RaceGUI()
{
    delete m_multitouch_gui;
}   // ~Racegui


//-----------------------------------------------------------------------------
void RaceGUI::init()
{
    RaceGUIBase::init();
    // Technically we only need getNumLocalPlayers, but using the
    // global kart id to find the data for a specific kart.
    int n = RaceManager::get()->getNumberOfKarts();

    m_animation_states.resize(n);
    m_rank_animation_duration.resize(n);
    m_last_ranks.resize(n);
}   // init

//-----------------------------------------------------------------------------
/** Reset the gui before a race. It initialised all rank animation related
 *  values back to the default.
 */
void RaceGUI::reset()
{
    RaceGUIBase::reset();
    for(unsigned int i=0; i<RaceManager::get()->getNumberOfKarts(); i++)
    {
        m_animation_states[i] = AS_NONE;
        m_last_ranks[i]       = i+1;
    }
}  // reset

//-----------------------------------------------------------------------------
void RaceGUI::calculateMinimapSize()
{
    float map_size_splitscreen = 1.0f;

    // If there are four players or more in splitscreen
    // and the map is in a player view, scale down the map
    if (RaceManager::get()->getNumLocalPlayers() >= 4 && !RaceManager::get()->getIfEmptyScreenSpaceExists())
    {
        // If the resolution is wider than 4:3, we don't have to scaledown the minimap as much
        // Uses some margin, in case the game's screen is not exactly 4:3
        if ( ((float) irr_driver->getFrameSize().Width / (float) irr_driver->getFrameSize().Height) >
             (4.1f/3.0f))
        {
            if (RaceManager::get()->getNumLocalPlayers() == 4)
                map_size_splitscreen = 0.75f;
            else
                map_size_splitscreen = 0.5f;
        }
        else
            map_size_splitscreen = 0.5f;
    }

    // Originally m_map_height was 100, and we take 480 as minimum res
    float scaling = std::min(irr_driver->getFrameSize().Height,  
                             irr_driver->getFrameSize().Width) / 480.0f;
    const float map_size = stk_config->m_minimap_size * map_size_splitscreen;
    const float top_margin = 3.5f * m_font_height;
    
    // Check if we have enough space for minimap when touch steering is enabled
    if (m_multitouch_gui != NULL  && !m_multitouch_gui->isSpectatorMode())
    {
        const float map_bottom = (float)(irr_driver->getActualScreenSize().Height - 
                                         m_multitouch_gui->getHeight());
        
        if ((map_size + 20.0f) * scaling > map_bottom - top_margin)
        {
            scaling = (map_bottom - top_margin) / (map_size + 20.0f);
        }
        
        // Use some reasonable minimum scale, because minimap size can be 
        // changed during the race
        scaling = std::max(scaling,
                           irr_driver->getActualScreenSize().Height * 0.15f / 
                           (map_size + 20.0f));
    }
    
    // Marker texture has to be power-of-two for (old) OpenGL compliance
    //m_marker_rendered_size  =  2 << ((int) ceil(1.0 + log(32.0 * scaling)));
    m_minimap_ai_size       = (int)( stk_config->m_minimap_ai_icon     * scaling);
    m_minimap_player_size   = (int)( stk_config->m_minimap_player_icon * scaling);
    m_map_width             = (int)(map_size * scaling);
    m_map_height            = (int)(map_size * scaling);

    if ((UserConfigParams::m_minimap_display == 1 && /*map on the right side*/
       RaceManager::get()->getNumLocalPlayers() == 1) || m_multitouch_gui)
    {
        m_map_left          = (int)(irr_driver->getActualScreenSize().Width - 
                                                        m_map_width - 10.0f*scaling);
        m_map_bottom        = (int)(3*irr_driver->getActualScreenSize().Height/4 - 
                                                        m_map_height);
    }
    else if ((UserConfigParams::m_minimap_display == 3 && /*map on the center of the screen*/
       RaceManager::get()->getNumLocalPlayers() == 1) || m_multitouch_gui)
    {
        m_map_left          = (int)(irr_driver->getActualScreenSize().Width / 2);
        if (m_map_left + m_map_width > (int)irr_driver->getActualScreenSize().Width)
          m_map_left        = (int)(irr_driver->getActualScreenSize().Width - m_map_width);
        m_map_bottom        = (int)( 10.0f * scaling);
    }
    else // default, map in the bottom-left corner
    {
        m_map_left          = (int)( 10.0f * scaling);
        m_map_bottom        = (int)( 10.0f * scaling);
    }

    // Minimap is also rendered bigger via OpenGL, so find power-of-two again
    const int map_texture   = 2 << ((int) ceil(1.0 + log(128.0 * scaling)));
    m_map_rendered_width    = map_texture;
    m_map_rendered_height   = map_texture;


    // special case : when 3 players play, use available 4th space for such things
    if (RaceManager::get()->getIfEmptyScreenSpaceExists())
    {
        m_map_left = irr_driver->getActualScreenSize().Width -
                     m_map_width - (int)( 10.0f * scaling);
        m_map_bottom        = (int)( 10.0f * scaling);
    }
    else if (m_multitouch_gui != NULL  && !m_multitouch_gui->isSpectatorMode())
    {
        m_map_left = (int)((irr_driver->getActualScreenSize().Width - 
                                                        m_map_width) * 0.95f);
        m_map_bottom = (int)(irr_driver->getActualScreenSize().Height - 
                                                    top_margin - m_map_height);
    }
}  // calculateMinimapSize

//-----------------------------------------------------------------------------
/** Render all global parts of the race gui, i.e. things that are only
 *  displayed once even in splitscreen.
 *  \param dt Timestep sized.
 */
void RaceGUI::renderGlobal(float dt)
{
#ifndef SERVER_ONLY
    RaceGUIBase::renderGlobal(dt);
    cleanupMessages(dt);

    // Special case : when 3 players play, use 4th window to display such
    // stuff (but we must clear it)
    if (RaceManager::get()->getIfEmptyScreenSpaceExists() &&
        !GUIEngine::ModalDialog::isADialogActive())
    {
        static video::SColor black = video::SColor(255,0,0,0);

        GL32_draw2DRectangle(black, irr_driver->getSplitscreenWindow(
            RaceManager::get()->getNumLocalPlayers()));
    }

    World *world = World::getWorld();
    assert(world != NULL);
    if(world->getPhase() >= WorldStatus::WAIT_FOR_SERVER_PHASE &&
       world->getPhase() <= WorldStatus::GO_PHASE      )
    {
        drawGlobalReadySetGo();
    }
    else if (world->isGoalPhase())
        drawGlobalGoal();

    if (!m_enabled) return;

    // Display the story mode timer if not in speedrun mode
    // If in speedrun mode, it is taken care of in GUI engine
    // as it must be displayed in all the game's screens
    if (UserConfigParams::m_display_story_mode_timer &&
        !UserConfigParams::m_speedrun_mode &&
        RaceManager::get()->raceWasStartedFromOverworld())
        irr_driver->displayStoryModeTimer();

    // MiniMap is drawn when the players wait for the start countdown to end
    drawGlobalMiniMap();

    // Timer etc. are not displayed unless the game is actually started.
    if(!world->isRacePhase()) return;

    //drawGlobalTimer checks if it should display in the current phase/mode
    FontDrawer::startBatching();
    drawGlobalTimer();

    if (!m_is_tutorial)
    {
        if (RaceManager::get()->isLinearRaceMode() &&
            RaceManager::get()->hasGhostKarts() &&
            RaceManager::get()->getNumberOfKarts() >= 2 )
            drawLiveDifference();

        if(world->getPhase() == WorldStatus::GO_PHASE ||
           world->getPhase() == WorldStatus::MUSIC_PHASE)
        {
            drawGlobalMusicDescription();
        }
    }

    if (!m_is_tutorial)
    {
        if (m_multitouch_gui != NULL)
        {
            drawGlobalPlayerIcons(m_multitouch_gui->getHeight());
        }
        else if (UserConfigParams::m_minimap_display == 0 || /*map in the bottom-left*/
                (UserConfigParams::m_minimap_display == 1 &&
                RaceManager::get()->getNumLocalPlayers() >= 2))
        {
            drawGlobalPlayerIcons(m_map_height + m_map_bottom);
        }
        else // map hidden or on the right side
        {
            drawGlobalPlayerIcons(0);
        }
    }
    FontDrawer::endBatching();
#endif
}   // renderGlobal

//-----------------------------------------------------------------------------
/** Render the details for a single player, i.e. speed, energy,
 *  collectibles, ...
 *  \param kart Pointer to the kart for which to render the view.
 */
void RaceGUI::renderPlayerView(const Camera *camera, float dt)
{
#ifndef SERVER_ONLY
    if (!m_enabled) return;

    RaceGUIBase::renderPlayerView(camera, dt);
    
    const core::recti &viewport = camera->getViewport();

    core::vector2df scaling = camera->getScaling();
    const AbstractKart *kart = camera->getKart();
    if(!kart) return;

    bool isSpectatorCam = Camera::getActiveCamera()->isSpectatorMode();

    if (!isSpectatorCam) drawPlungerInFace(camera, dt);

    if (viewport.getWidth() != (int)irr_driver->getActualScreenSize().Width)
    {
        scaling *= float(viewport.getWidth()) / float(irr_driver->getActualScreenSize().Width); // scale race GUI along screen size
    }
    else
    {
        scaling *= float(viewport.getWidth()) / 800.0f; // scale race GUI along screen size
    }
    
    drawAllMessages(kart, viewport, scaling);

    if(!World::getWorld()->isRacePhase()) return;

    FontDrawer::startBatching();
    if (Camera::getActiveCamera()->getMode() != Camera::CM_SPECTATOR_TOP_VIEW)
    {
        if (m_multitouch_gui == NULL || m_multitouch_gui->isSpectatorMode())
        {
            drawPowerupIcons(kart, viewport, scaling);
            drawSpeedEnergyRank(kart, viewport, scaling, dt);
        }
    }

    if (!m_is_tutorial)
        drawLap(kart, viewport, scaling);

    // draw heading line
    drawHeadingLine(kart,20);

    // draw ball line
    if (RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_SOCCER)
    {
        drawBallLine(10);
    }

    FontDrawer::endBatching();
#endif
}   // renderPlayerView

//-----------------------------------------------------------------------------
/** Displays the racing time on the screen.
 */
void RaceGUI::drawGlobalTimer()
{
    assert(World::getWorld() != NULL);

    if (!World::getWorld()->shouldDrawTimer())
    {
        return;
    }

    core::stringw sw;
    video::SColor time_color = video::SColor(255, 255, 255, 255);
    int dist_from_right = 10 + m_timer_width+20;

    bool use_digit_font = true;

    float elapsed_time = World::getWorld()->getTime();
    if (!RaceManager::get()->hasTimeTarget() ||
        RaceManager::get()->getMinorMode() ==RaceManager::MINOR_MODE_SOCCER ||
        RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_FREE_FOR_ALL ||
        RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_CAPTURE_THE_FLAG)
    {
        sw = core::stringw (
            StringUtils::timeToString(elapsed_time).c_str() );
    }
    else
    {
        float time_target = RaceManager::get()->getTimeTarget();
        if (elapsed_time < time_target)
        {
            sw = core::stringw (
              StringUtils::timeToString(time_target - elapsed_time).c_str());
        }
        else
        {
            sw = _("Challenge Failed");
            int string_width =
                GUIEngine::getFont()->getDimension(sw.c_str()).Width;
            dist_from_right = 10 + string_width;
            time_color = video::SColor(255,255,0,0);
            use_digit_font = false;
        }
    }

    core::rect<s32> pos(irr_driver->getActualScreenSize().Width - dist_from_right,
                        irr_driver->getActualScreenSize().Height*2/100,
                        irr_driver->getActualScreenSize().Width,
                        irr_driver->getActualScreenSize().Height*6/100);

    // special case : when 3 players play, use available 4th space for such things
    if (RaceManager::get()->getIfEmptyScreenSpaceExists())
    {
        pos -= core::vector2d<s32>(0, pos.LowerRightCorner.Y / 2);
        pos += core::vector2d<s32>(0, irr_driver->getActualScreenSize().Height - irr_driver->getSplitscreenWindow(0).getHeight());
    }

    gui::ScalableFont* font = (use_digit_font ? GUIEngine::getHighresDigitFont() : GUIEngine::getFont());
    font->setScale(1.0f);
    font->setBlackBorder(true);
    font->draw(sw, pos, time_color, false, false, NULL,
               true /* ignore RTL */);
    font->setBlackBorder(false);

}   // drawGlobalTimer


//-----------------------------------------------------------------------------
/** Displays the live difference with a ghost on screen.
 */
void RaceGUI::drawLiveDifference()
{
    assert(World::getWorld() != NULL);

    if (!World::getWorld()->shouldDrawTimer())
    {
        return;
    }

    const LinearWorld *linearworld = dynamic_cast<LinearWorld*>(World::getWorld());
    assert(linearworld != NULL);

    // Don't display the live difference timer if its time is wrong
    // (before crossing the start line at start or after crossing it at end)
    if (!linearworld->hasValidTimeDifference())
        return;

    float live_difference = linearworld->getLiveTimeDifference();

    int timer_width = m_small_precise_timer_width;
    
    // 59.9995 is the smallest number of seconds that could get rounded to 1 minute
    // when rounding at the closest ms
    if (fabsf(live_difference) >= 59.9995f)
        timer_width = m_big_precise_timer_width;

    if (live_difference < 0.0f)
        timer_width += m_negative_timer_additional_width;

    core::stringw sw;
    video::SColor time_color;

    // Change color depending on value
    if (live_difference > 1.0f)
        time_color = video::SColor(255, 255, 0, 0);
    else if (live_difference > 0.0f)
        time_color = video::SColor(255, 255, 160, 0);
    else if (live_difference > -1.0f)
        time_color = video::SColor(255, 160, 255, 0);
    else
        time_color = video::SColor(255, 0, 255, 0);

    int dist_from_right = 10 + timer_width;

    sw = core::stringw (StringUtils::timeToString(live_difference,3,
                        /* display_minutes_if_zero */ false).c_str() );

    core::rect<s32> pos(irr_driver->getActualScreenSize().Width - dist_from_right,
                        irr_driver->getActualScreenSize().Height*7/100,
                        irr_driver->getActualScreenSize().Width,
                        irr_driver->getActualScreenSize().Height*11/100);

    gui::ScalableFont* font = GUIEngine::getHighresDigitFont();
    font->setScale(1.0f);
    font->setBlackBorder(true);
    font->draw(sw.c_str(), pos, time_color, false, false, NULL,
               true /* ignore RTL */);
    font->setBlackBorder(false);
}   // drawLiveDifference

//-----------------------------------------------------------------------------
/** Draws the mini map and the position of all karts on it.
 */
void RaceGUI::drawGlobalMiniMap()
{
#ifndef SERVER_ONLY
    //TODO : exception for some game modes ? Another option "Hidden in race, shown in battle ?"
    if (UserConfigParams::m_minimap_display == 2) /*map hidden*/
        return;
    
    if (m_multitouch_gui != NULL && !m_multitouch_gui->isSpectatorMode())
    {
        float max_scale = 1.3f;
                                                      
        if (UserConfigParams::m_multitouch_scale > max_scale)
            return;
    }

    // draw a map when arena has a navigation mesh.
    Track *track = Track::getCurrentTrack();
    if ( (track->isArena() || track->isSoccer()) && !(track->hasNavMesh()) )
        return;

    int upper_y = irr_driver->getActualScreenSize().Height - m_map_bottom - m_map_height;
    int lower_y = irr_driver->getActualScreenSize().Height - m_map_bottom;

    core::rect<s32> dest(m_map_left, upper_y,
                         m_map_left + m_map_width, lower_y);

    track->drawMiniMap(dest);

    World* world = World::getWorld();
    CaptureTheFlag *ctf_world = dynamic_cast<CaptureTheFlag*>(World::getWorld());
    SoccerWorld *soccer_world = dynamic_cast<SoccerWorld*>(World::getWorld());

    if (ctf_world)
    {
        Vec3 draw_at;
        if (!ctf_world->isRedFlagInBase())
        {
            track->mapPoint2MiniMap(Track::getCurrentTrack()->getRedFlag().getOrigin(),
                &draw_at);
            core::rect<s32> rs(core::position2di(0, 0), m_red_flag->getSize());
            core::rect<s32> rp(m_map_left+(int)(draw_at.getX()-(m_minimap_player_size/1.4f)),
                lower_y   -(int)(draw_at.getY()+(m_minimap_player_size/2.2f)),
                m_map_left+(int)(draw_at.getX()+(m_minimap_player_size/1.4f)),
                lower_y   -(int)(draw_at.getY()-(m_minimap_player_size/2.2f)));
            draw2DImage(m_red_flag, rp, rs, NULL, NULL, true, true);
        }
        Vec3 pos = ctf_world->getRedHolder() == -1 ? ctf_world->getRedFlag() :
            ctf_world->getKart(ctf_world->getRedHolder())->getSmoothedTrans().getOrigin();

        track->mapPoint2MiniMap(pos, &draw_at);
        core::rect<s32> rs(core::position2di(0, 0), m_red_flag->getSize());
        core::rect<s32> rp(m_map_left+(int)(draw_at.getX()-(m_minimap_player_size/1.4f)),
                                 lower_y   -(int)(draw_at.getY()+(m_minimap_player_size/2.2f)),
                                 m_map_left+(int)(draw_at.getX()+(m_minimap_player_size/1.4f)),
                                 lower_y   -(int)(draw_at.getY()-(m_minimap_player_size/2.2f)));
        draw2DImage(m_red_flag, rp, rs, NULL, NULL, true);

        if (!ctf_world->isBlueFlagInBase())
        {
            track->mapPoint2MiniMap(Track::getCurrentTrack()->getBlueFlag().getOrigin(),
                &draw_at);
            core::rect<s32> rs(core::position2di(0, 0), m_blue_flag->getSize());
            core::rect<s32> rp(m_map_left+(int)(draw_at.getX()-(m_minimap_player_size/1.4f)),
                lower_y   -(int)(draw_at.getY()+(m_minimap_player_size/2.2f)),
                m_map_left+(int)(draw_at.getX()+(m_minimap_player_size/1.4f)),
                lower_y   -(int)(draw_at.getY()-(m_minimap_player_size/2.2f)));
            draw2DImage(m_blue_flag, rp, rs, NULL, NULL, true, true);
        }

        pos = ctf_world->getBlueHolder() == -1 ? ctf_world->getBlueFlag() :
            ctf_world->getKart(ctf_world->getBlueHolder())->getSmoothedTrans().getOrigin();

        track->mapPoint2MiniMap(pos, &draw_at);
        core::rect<s32> bs(core::position2di(0, 0), m_blue_flag->getSize());
        core::rect<s32> bp(m_map_left+(int)(draw_at.getX()-(m_minimap_player_size/1.4f)),
                                 lower_y   -(int)(draw_at.getY()+(m_minimap_player_size/2.2f)),
                                 m_map_left+(int)(draw_at.getX()+(m_minimap_player_size/1.4f)),
                                 lower_y   -(int)(draw_at.getY()-(m_minimap_player_size/2.2f)));
        draw2DImage(m_blue_flag, bp, bs, NULL, NULL, true);
    }

    // show items and nitros on minimap
    if (UserConfigParams::m_karts_powerup_gui)
    {
        ItemManager* itm = track->getItemManager();
        int marker_half_size = m_minimap_ai_size>>1;
        for (unsigned i = 0; i < itm->getNumberOfItems(); i++)
        {
            ItemState* it = itm->getItem(i);
            if (it != NULL) //&& it->getType() != Item::ITEM_EASTER_EGG // hide easter egg
            {
                if (it->isAvailable()) // only show collectable items
                {
                    video::ITexture *icon_item = itm->getIcon(it->getType())->getTexture();
                    assert(icon_item);
                    if (icon_item != NULL)
                    {
                        const Vec3& xyz = it->getXYZ().toIrrVector();
                        Vec3 draw_at;
                        track->mapPoint2MiniMap(xyz, &draw_at);
                        // make item icon half the player icon size, otherwise too busy
                        core::rect<s32> pos_tmp(m_map_left+(int)(draw_at.getX()-marker_half_size/2), // left
                                                lower_y   -(int)(draw_at.getY()+marker_half_size/2), // upper
                                                m_map_left+(int)(draw_at.getX()+marker_half_size/2), // right
                                                lower_y   -(int)(draw_at.getY()-marker_half_size/2));// lower
                        core::rect<s32> source(core::position2di(0, 0), icon_item->getSize());
                        draw2DImage(icon_item, pos_tmp, source, NULL, NULL, true);
                    }
                }
            }
        }
    }

    AbstractKart* target_kart = NULL;
    Camera* cam = Camera::getActiveCamera();
    auto cl = LobbyProtocol::get<ClientLobby>();
    bool is_nw_spectate = cl && cl->isSpectator();
    // For network spectator highlight
    if (RaceManager::get()->getNumLocalPlayers() == 1 && cam && is_nw_spectate)
        target_kart = cam->getKart();

    // Move AI/remote players to the beginning, so that local players icons
    // are drawn above them
    World::KartList karts = world->getKarts();
    std::partition(karts.begin(), karts.end(), [target_kart, is_nw_spectate]
        (const std::shared_ptr<AbstractKart>& k)->bool
    {
        if (is_nw_spectate)
            return k.get() != target_kart;
        else
            return !k->getController()->isLocalPlayerController();
    });

    for (unsigned int i = 0; i < karts.size(); i++)
    {
        const AbstractKart *kart = karts[i].get();
        const SpareTireAI* sta =
            dynamic_cast<const SpareTireAI*>(kart->getController());

        // don't draw eliminated kart
        if (kart->isEliminated() && !(sta && sta->isMoving())) 
            continue;
        if (!kart->isVisible())
            continue;
        const Vec3& xyz = kart->getSmoothedTrans().getOrigin();
        Vec3 draw_at;
        track->mapPoint2MiniMap(xyz, &draw_at);

        video::ITexture* icon = sta ? m_heart_icon :
            kart->getKartProperties()->getMinimapIcon();
        if (icon == NULL)
        {
            continue;
        }
        bool is_local = is_nw_spectate ? kart == target_kart :
            kart->getController()->isLocalPlayerController();
        // int marker_height = m_marker->getSize().Height;
        core::rect<s32> source(core::position2di(0, 0), icon->getSize());
        int marker_half_size = (is_local
                                ? m_minimap_player_size
                                : m_minimap_ai_size                        )>>1;
        core::rect<s32> position(m_map_left+(int)(draw_at.getX()-marker_half_size),
                                 lower_y   -(int)(draw_at.getY()+marker_half_size),
                                 m_map_left+(int)(draw_at.getX()+marker_half_size),
                                 lower_y   -(int)(draw_at.getY()-marker_half_size));

        bool has_teams = (ctf_world || soccer_world);
        
        // Highlight the player icons with some background image.
        if ((has_teams || is_local) && m_icons_frame != NULL)
        {
            video::SColor color = kart->getKartProperties()->getColor();
            
            if (has_teams)
            {
                KartTeam team = world->getKartTeam(kart->getWorldKartId());
                
                if (team == KART_TEAM_RED)
                {
                    color = video::SColor(255, 200, 0, 0);
                }
                else if (team == KART_TEAM_BLUE)
                {
                    color = video::SColor(255, 0, 0, 200);
                }
            }
                                  
            video::SColor colors[4] = {color, color, color, color};

            const core::rect<s32> rect(core::position2d<s32>(0,0),
                                        m_icons_frame->getSize());

            // show kart direction
            // Find the direction a kart is moving in
            btTransform trans = kart->getTrans();
            Vec3 direction(trans.getBasis().getColumn(2));
            // Get the rotation to rotate the icon frame
            float rotation = atan2f(direction.getZ(),direction.getX());
            if (track->getMinimapInvert())
            {   // correct the direction due to invert minimap for blue
                rotation = rotation + M_PI;
            }
            rotation = -1.0f * rotation + 0.25f * M_PI; // icons-frame_arrow.png was rotated by 45 degrees
            draw2DImage(m_icons_frame, position, rect, NULL, colors, true, false, rotation);

        }   // if isPlayerController

        // icon squash with kart squash
        if (UserConfigParams::m_karts_powerup_gui &&
            !kart->getKartAnimation() && kart->isSquashed() )
        {
            core::rect<s32> pos_tmp(m_map_left+(int)(draw_at.getX()-marker_half_size),   // left
                                    lower_y   -(int)(draw_at.getY()+marker_half_size/2), // upper
                                    m_map_left+(int)(draw_at.getX()+marker_half_size),   // right
                                    lower_y   -(int)(draw_at.getY()-marker_half_size/2));// lower
            draw2DImage(icon, pos_tmp, source, NULL, NULL, true);
        }
        else
        {
            draw2DImage(icon, position, source, NULL, NULL, true);
        }

        // show player's powerups in minimap
        const Powerup* powerup = kart->getPowerup();
        if (UserConfigParams::m_karts_powerup_gui &&
            powerup->getType() != PowerupManager::POWERUP_NOTHING && !kart->hasFinishedRace())
        {
            video::ITexture *iconItem = powerup->getIcon()->getTexture();
            assert(iconItem);
            const core::rect<s32> posItem(m_map_left+(int)(draw_at.getX()), // left
                                          lower_y   -(int)(draw_at.getY()), // upper
                                          m_map_left+(int)(draw_at.getX()+marker_half_size*3/2),  // right
                                          lower_y   -(int)(draw_at.getY()-marker_half_size*3/2)); // lower
            const core::rect<s32> rect(core::position2d<s32>(0,0), iconItem->getSize());
            draw2DImage(iconItem, posItem, rect, NULL, NULL, true);

            int numberItems = kart->getPowerup()->getNum();
            if (numberItems > 1)
            {
                gui::ScalableFont* font = GUIEngine::getHighresDigitFont();
                const core::rect<s32> posNumber(m_map_left+(int)(draw_at.getX()+marker_half_size),     // left
                                                lower_y   -(int)(draw_at.getY()-marker_half_size/2),   // upper
                                                m_map_left+(int)(draw_at.getX()+marker_half_size*5/2), // right
                                                lower_y   -(int)(draw_at.getY()-marker_half_size*4/2));// lower
                font->setScale(3.f*((float) marker_half_size)/(2.f*(float)font->getDimension(L"X").Height));
                font->draw(StringUtils::toWString(numberItems), posNumber, video::SColor(255, 255, 255, 255));
            }
        }
        // show player's attachment
        if (UserConfigParams::m_karts_powerup_gui &&
            kart->getAttachment()->getType() != Attachment::ATTACH_NOTHING)
        {
            video::ITexture *icon_attachment =
            attachment_manager->getIcon(kart->getAttachment()->getType())
            ->getTexture();
            if (icon_attachment != NULL)
            {
                const core::rect<s32> posAttach(m_map_left+(int)(draw_at.getX()-marker_half_size*3/2), // left
                                                lower_y   -(int)(draw_at.getY()+marker_half_size*3/2), // upper
                                                m_map_left+(int)(draw_at.getX()),  // right
                                                lower_y   -(int)(draw_at.getY())); // lower
                const core::rect<s32> rect(core::position2d<s32>(0,0), icon_attachment->getSize());
                draw2DImage(icon_attachment, posAttach, rect, NULL, NULL, true);
            }
        }
        // show player's name initials
        if (UserConfigParams::m_karts_powerup_gui &&
            !kart->getName().empty())
        {
            gui::ScalableFont* font = GUIEngine::getHighresDigitFont();
            const core::rect<s32> posNumber(m_map_left+(int)(draw_at.getX()+marker_half_size/4.0),  // left
                                            lower_y   -(int)(draw_at.getY()+marker_half_size*3.0/2),// upper
                                            m_map_left+(int)(draw_at.getX()+marker_half_size*3.0/2),// right
                                            lower_y   -(int)(draw_at.getY()+marker_half_size/4.0)); // lower
            font->setScale(5.0f*(float) marker_half_size/(4.0f*(float)font->getDimension(L"X").Height));
            font->draw(kart->getController()->getName().subString(0,1), posNumber, video::SColor(255, 255, 255, 255));
        }

    }   // for i<getNumKarts

    // Draw checklines on the minimap
    if (RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_NORMAL_RACE ||
        RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_TIME_TRIAL ||
        RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_FOLLOW_LEADER)
    {
        // Loop all checklines
        core::rect<s32> rect(core::position2di(0, 0), m_checkline_icon->getSize());
        CheckManager* cm = Track::getCurrentTrack()->getCheckManager();
        for (unsigned i = 0; i < cm->getCheckStructureCount(); i++)
        {
            CheckStructure* cs = cm->getCheckStructure(i);
            if(cs->getType() == CheckStructure::CT_ACTIVATE)
            {
                CheckLine* cl = dynamic_cast<CheckLine*>(cs);

                // get the two points that define the checkline and map to minimap
                Vec3 p1, p2;
                track->mapPoint2MiniMap(cl->getLeftPoint() , &p1);
                track->mapPoint2MiniMap(cl->getRightPoint(), &p2);

                // draw the checkline
                const Vec3 direction = p2-p1;
                const Vec3 draw_at = 0.5f*(p1+p2);
                const float rotation = atan2f(direction.getY(),direction.getX());
                const s32 half_line_size = (s32)(0.5f*direction.length());
                core::rect<s32> position(m_map_left+(int)(draw_at.getX()-half_line_size),
                                         lower_y   -(int)(draw_at.getY()+half_line_size),
                                         m_map_left+(int)(draw_at.getX()+half_line_size),
                                         lower_y   -(int)(draw_at.getY()-half_line_size));

                draw2DImage(m_checkline_icon, position, rect, NULL, NULL, true, false, -1.0f*rotation);
            }
        }
    }

    // Draw the basket-ball icons on the minimap
    std::vector<Vec3> basketballs = ProjectileManager::get()->getBasketballPositions();
    for(unsigned int i = 0; i != basketballs.size(); i++)
    {
        Vec3 draw_at;
        track->mapPoint2MiniMap(basketballs[i], &draw_at);

        video::ITexture* icon = m_basket_ball_icon;

        core::rect<s32> source(core::position2di(0, 0), icon->getSize());
        int marker_half_size = m_minimap_player_size / 4;
        core::rect<s32> position(m_map_left+(int)(draw_at.getX()-marker_half_size),
                                 lower_y   -(int)(draw_at.getY()+marker_half_size),
                                 m_map_left+(int)(draw_at.getX()+marker_half_size),
                                 lower_y   -(int)(draw_at.getY()-marker_half_size));

        draw2DImage(icon, position, source, NULL, NULL, true);
    }

    // Draw the soccer ball icon
    if (soccer_world)
    {
        Vec3 draw_at;
        track->mapPoint2MiniMap(soccer_world->getBallPosition(), &draw_at);

        core::rect<s32> source(core::position2di(0, 0), m_soccer_ball->getSize());
        core::rect<s32> position(m_map_left+(int)(draw_at.getX()-(m_minimap_player_size/2.5f)),
                                 lower_y   -(int)(draw_at.getY()+(m_minimap_player_size/2.5f)),
                                 m_map_left+(int)(draw_at.getX()+(m_minimap_player_size/2.5f)),
                                 lower_y   -(int)(draw_at.getY()-(m_minimap_player_size/2.5f)));
        draw2DImage(m_soccer_ball, position, source, NULL, NULL, true);
    }
#endif
}   // drawGlobalMiniMap

//-----------------------------------------------------------------------------
/** Energy meter that gets filled with nitro. This function is called from
 *  drawSpeedEnergyRank, which defines the correct position of the energy
 *  meter.
 *  \param x X position of the meter.
 *  \param y Y position of the meter.
 *  \param kart Kart to display the data for.
 *  \param scaling Scaling applied (in case of split screen)
 */
void RaceGUI::drawEnergyMeter(int x, int y, const AbstractKart *kart,
                              const core::recti &viewport,
                              const core::vector2df &scaling)
{
#ifndef SERVER_ONLY
    float min_ratio        = std::min(scaling.X, scaling.Y);
    const int GAUGEWIDTH   = 94;//same inner radius as the inner speedometer circle
    int gauge_width        = (int)(GAUGEWIDTH*min_ratio);
    int gauge_height       = (int)(GAUGEWIDTH*min_ratio);

    float state = (float)(kart->getEnergy())
                / kart->getKartProperties()->getNitroMax();
    if (state < 0.0f) state = 0.0f;
    else if (state > 1.0f) state = 1.0f;

    core::vector2df offset;
    offset.X = (float)(x-gauge_width) - 9.5f*scaling.X;
    offset.Y = (float)y-11.5f*scaling.Y;


    // Background
    draw2DImage(m_gauge_empty, core::rect<s32>((int)offset.X,
                                               (int)offset.Y-gauge_height,
                                               (int)offset.X + gauge_width,
                                               (int)offset.Y) /* dest rect */,
                core::rect<s32>(core::position2d<s32>(0,0),
                                m_gauge_empty->getSize()) /* source rect */,
                NULL /* clip rect */, NULL /* colors */,
                true /* alpha */);

    // The positions for A to G are defined here.
    // They are calculated from gauge_full.png
    // They are further than the nitrometer farther position because
    // the lines between them would otherwise cut through the outside circle.
    
    const int vertices_count = 9;

    core::vector2df position[vertices_count];
    position[0].X = 0.324f;//A
    position[0].Y = 0.35f;//A
    position[1].X = 0.01f;//B1 (margin for gauge goal)
    position[1].Y = 0.88f;//B1
    position[2].X = 0.029f;//B2
    position[2].Y = 0.918f;//B2
    position[3].X = 0.307f;//C
    position[3].Y = 0.99f;//C
    position[4].X = 0.589f;//D
    position[4].Y = 0.932f;//D
    position[5].X = 0.818f;//E
    position[5].Y = 0.755f;//E
    position[6].X = 0.945f;//F
    position[6].Y = 0.497f;//F
    position[7].X = 0.948f;//G1
    position[7].Y = 0.211f;//G1
    position[8].X = 0.94f;//G2 (margin for gauge goal)
    position[8].Y = 0.17f;//G2

    // The states at which different polygons must be used.

    float threshold[vertices_count-2];
    threshold[0] = 0.0001f; //for gauge drawing
    threshold[1] = 0.2f;
    threshold[2] = 0.4f;
    threshold[3] = 0.6f;
    threshold[4] = 0.8f;
    threshold[5] = 0.9999f;
    threshold[6] = 1.0f;

    // Filling (current state)

    if (state > 0.0f)
    {
        video::S3DVertex vertices[vertices_count];

        //3D effect : wait for the full border to appear before drawing
        for (int i=0;i<5;i++)
        {
            if ((state-0.2f*i < 0.006f && state-0.2f*i >= 0.0f) || (0.2f*i-state < 0.003f && 0.2f*i-state >= 0.0f) )
            {
                state = 0.2f*i-0.003f;
                break;
            }
        }

        unsigned int count = computeVerticesForMeter(position, threshold, vertices, vertices_count,
                                                     state, gauge_width, gauge_height, offset);

        if(kart->getControls().getNitro() || kart->isOnMinNitroTime())
            drawMeterTexture(m_gauge_full_bright, vertices, count);
        else
            drawMeterTexture(m_gauge_full, vertices, count);
    }

    // Target

    if (RaceManager::get()->getCoinTarget() > 0)
    {
        float coin_target = (float)RaceManager::get()->getCoinTarget()
                          / kart->getKartProperties()->getNitroMax();

        video::S3DVertex vertices[vertices_count];

        unsigned int count = computeVerticesForMeter(position, threshold, vertices, vertices_count, 
                                                     coin_target, gauge_width, gauge_height, offset);

        drawMeterTexture(m_gauge_goal, vertices, count);
    }
#endif
}   // drawEnergyMeter

//-----------------------------------------------------------------------------
/** Draws the rank of a player.
 *  \param kart The kart of the player.
 *  \param offset Offset of top left corner for this display (for splitscreen).
 *  \param min_ratio Scaling of the screen (for splitscreen).
 *  \param meter_width Width of the meter (inside which the rank is shown).
 *  \param meter_height Height of the meter (inside which the rank is shown).
 *  \param dt Time step size.
 */
void RaceGUI::drawRank(const AbstractKart *kart,
                      const core::vector2df &offset,
                      float min_ratio, int meter_width,
                      int meter_height, float dt)
{
    static video::SColor color = video::SColor(255, 255, 255, 255);

    // Draw rank
    WorldWithRank *world = dynamic_cast<WorldWithRank*>(World::getWorld());
    if (!world || !world->displayRank())
        return;

    int id = kart->getWorldKartId();

    if (m_animation_states[id] == AS_NONE)
    {
        if (m_last_ranks[id] != kart->getPosition())
        {
            m_rank_animation_duration[id] = 0.0f;
            m_animation_states[id] = AS_SMALLER;
        }
    }
    else
    {
        m_rank_animation_duration[id] += dt;
    }

    float scale = 1.0f;
    int rank = kart->getPosition();
    const float DURATION = 0.4f;
    const float MIN_SHRINK = 0.3f;
    if (m_animation_states[id] == AS_SMALLER)
    {
        scale = 1.0f - m_rank_animation_duration[id]/ DURATION;
        rank = m_last_ranks[id];
        if (scale < MIN_SHRINK)
        {
            m_animation_states[id] = AS_BIGGER;
            m_rank_animation_duration[id] = 0.0f;
            // Store the new rank
            m_last_ranks[id] = kart->getPosition();
            scale = MIN_SHRINK;
        }
    }
    else if (m_animation_states[id] == AS_BIGGER)
    {
        scale = m_rank_animation_duration[id] / DURATION + MIN_SHRINK;
        rank = m_last_ranks[id];
        if (scale > 1.0f)
        {
            m_animation_states[id] = AS_NONE;
            scale = 1.0f;
        }

    }
    else
    {
        m_last_ranks[id] = kart->getPosition();
    }

    gui::ScalableFont* font = GUIEngine::getHighresDigitFont();
    
    int font_height = font->getDimension(L"X").Height;
    font->setScale((float)meter_height / font_height * 0.4f * scale);
    std::ostringstream oss;
    oss << rank; // the current font has no . :(   << ".";

    core::recti pos;
    pos.LowerRightCorner = core::vector2di(int(offset.X + 0.65f*meter_width), int(offset.Y - 2.5f*meter_height));
    pos.UpperLeftCorner = core::vector2di(int(offset.X + 0.65f*meter_width), int(offset.Y - 2.5f*meter_height));

    //static video::SColor color = video::SColor(255, 255, 255, 255);
    if (RaceManager::get()->getNumberOfKarts() > 1)
        font->draw(oss.str().c_str(), pos, color, true, true);

    font->setScale(1.0f);
}   // drawRank

//-----------------------------------------------------------------------------
/** Draws the speedometer, the display of available nitro, and
 *  the rank of the kart (inside the speedometer).
 *  \param kart The kart for which to show the data.
 *  \param viewport The viewport to use.
 *  \param scaling Which scaling to apply to the speedometer.
 *  \param dt Time step size.
 */
void RaceGUI::drawSpeedEnergyRank(const AbstractKart* kart,
                                 const core::recti &viewport,
                                 const core::vector2df &scaling,
                                 float dt)
{
#ifndef SERVER_ONLY
    float min_ratio         = std::min(scaling.X, scaling.Y);
    const int SPEEDWIDTH   = 128;
    int meter_width        = (int)(SPEEDWIDTH*min_ratio);
    int meter_height       = (int)(SPEEDWIDTH*min_ratio);

    drawEnergyMeter(viewport.LowerRightCorner.X ,
                    (int)(viewport.LowerRightCorner.Y),
                    kart, viewport, scaling);

    // First draw the meter (i.e. the background )
    // -------------------------------------------------------------------------
    core::vector2df offset;
    offset.X = (float)(viewport.LowerRightCorner.X-meter_width) - 24.0f*scaling.X;
    offset.Y = viewport.LowerRightCorner.Y-10.0f*scaling.Y;

    const core::rect<s32> meter_pos((int)offset.X,
                                    (int)(offset.Y-meter_height),
                                    (int)(offset.X+meter_width),
                                    (int)offset.Y);
    video::ITexture *meter_texture = m_speed_meter_icon->getTexture();
    const core::rect<s32> meter_texture_coords(core::position2d<s32>(0,0),
                                               meter_texture->getSize());
    draw2DImage(meter_texture, meter_pos, meter_texture_coords, NULL,
                       NULL, true);
    // TODO: temporary workaround, shouldn't have to use
    // draw2DVertexPrimitiveList to render a simple rectangle

    const float speed =  kart->getSpeed();

    drawRank(kart, offset, min_ratio, meter_width, meter_height, dt);

    if (RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_NORMAL_RACE ||
        RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_TIME_TRIAL ||
        RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_FOLLOW_LEADER)
    {
        drawNumericSpeed2(kart, offset, meter_width, meter_height);
    }
    else
    {
        drawNumericSpeed(kart, offset, meter_width, meter_height);
    }

    /*
    core::vector2df offset2;
    offset2.X = (float) viewport.getCenter().X;
    offset2.Y = (float) (viewport.getCenter().Y - 0.4*m_font_height);
    int hub_width = m_font_height*12*0.4;
    int hub_height = m_font_height*2*0.4;
    drawHubSpeed(kart,offset2,hub_width,hub_height);
    */

    if(speed <=0) return;  // Nothing to do if speed is negative.

    // Draw the actual speed bar (if the speed is >0)
    // ----------------------------------------------
    float speed_ratio = speed/40.0f; //max displayed speed of 40
    if(speed_ratio>1) speed_ratio = 1;

    // see computeVerticesForMeter for the detail of the drawing
    // If increasing this, update drawMeterTexture

    const int vertices_count = 12;

    video::S3DVertex vertices[vertices_count];

    // The positions for A to J2 are defined here.

    // They are calculated from speedometer.png
    // A is the center of the speedometer's circle
    // B2, C, D, E, F, G, H, I and J1 are points on the line
    // from A to their respective 1/8th threshold division
    // B2 is 36,9° clockwise from the vertical (on bottom-left)
    // J1 s 70,7° clockwise from the vertical (on upper-right)
    // B1 and J2 are used for correct display of the 3D effect
    // They are 1,13* further than the speedometer farther position because
    // the lines between them would otherwise cut through the outside circle.

    core::vector2df position[vertices_count];

    position[0].X = 0.546f;//A
    position[0].Y = 0.566f;//A
    position[1].X = 0.216f;//B1
    position[1].Y = 1.036f;//B1
    position[2].X = 0.201f;//B2
    position[2].Y = 1.023f;//B2
    position[3].X = 0.036f;//C
    position[3].Y = 0.831f;//C
    position[4].X = -0.029f;//D
    position[4].Y = 0.589f;//D
    position[5].X = 0.018f;//E
    position[5].Y = 0.337f;//E
    position[6].X = 0.169f;//F
    position[6].Y = 0.134f;//F
    position[7].X = 0.391f;//G
    position[7].Y = 0.014f;//G
    position[8].X = 0.642f;//H
    position[8].Y = 0.0f;//H
    position[9].X = 0.878f;//I
    position[9].Y = 0.098f;//I
    position[10].X = 1.046f;//J1
    position[10].Y = 0.285f;//J1
    position[11].X = 1.052f;//J2
    position[11].Y = 0.297f;//J2

    // The speed ratios at which different triangles must be used.

    float threshold[vertices_count-2];
    threshold[0] = 0.00001f;//for the 3D margin
    threshold[1] = 0.125f;
    threshold[2] = 0.25f;
    threshold[3] = 0.375f;
    threshold[4] = 0.50f;
    threshold[5] = 0.625f;
    threshold[6] = 0.750f;
    threshold[7] = 0.875f;
    threshold[8] = 0.99999f;//for the 3D margin
    threshold[9] = 1.0f;

    //3D effect : wait for the full border to appear before drawing
    for (int i=0;i<8;i++)
    {
        if ((speed_ratio-0.125f*i < 0.00625f && speed_ratio-0.125f*i >= 0.0f) || (0.125f*i-speed_ratio < 0.0045f && 0.125f*i-speed_ratio >= 0.0f) )
        {
            speed_ratio = 0.125f*i-0.0045f;
            break;
        }
    }

    unsigned int count = computeVerticesForMeter(position, threshold, vertices, vertices_count, 
                                                     speed_ratio, meter_width, meter_height, offset);


    drawMeterTexture(m_speed_bar_icon->getTexture(), vertices, count);
#endif
}   // drawSpeedEnergyRank

void RaceGUI::drawMeterTexture(video::ITexture *meter_texture, video::S3DVertex vertices[], unsigned int count)
{
#ifndef SERVER_ONLY
    //Should be greater or equal than the greatest vertices_count used by the meter functions
    if (count < 2)
        return;
    short int index[12];
    for(unsigned int i=0; i<count; i++)
    {
        index[i]=i;
        vertices[i].Color = video::SColor(255, 255, 255, 255);
    }

    video::SMaterial m;
    m.setTexture(0, meter_texture);
    m.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
    irr_driver->getVideoDriver()->setMaterial(m);
    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    draw2DVertexPrimitiveList(m.getTexture(0), vertices, count,
        index, count-2, video::EVT_STANDARD, scene::EPT_TRIANGLE_FAN);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);

#endif
}   // drawMeterTexture



//-----------------------------------------------------------------------------
/** This function computes a polygon used for drawing the measure for a meter (speedometer, etc.)
 *  The variable measured by the meter is compared to the thresholds, and is then used to
 *  compute a point between the two points associated with the lower and upper threshold
 *  Then, a polygon is calculated linking all the previous points and the variable point
 *  which link back to the first point. This polygon is used for drawing.
 *
 *  Consider the following example :
 *
 *      A                E
 *                      -|
 *                      x
 *                      |
 *                   -D-|
 *                -w-|
 *           |-C--|
 *     -B--v-|
 *
 *  If the measure is inferior to the first threshold, the function will create a triangle ABv
 *  with the position of v varying proportionally on a line between B and C ;
 *  at B with 0 and at C when it reaches the first threshold.
 *  If the measure is between the first and second thresholds, the function will create a quad ABCw,
 *  with w varying in the same way than v.
 *  If the measure exceds the higher threshold, the function will return the poly ABCDE.
 *  
 *  \param position The relative positions of the vertices.
 *  \param threshold The thresholds at which the variable point switch from a segment to the next.
 *                   The size of this array should be smaller by two than the position array.
 *                   The last threshold determines the measure over which the meter is full
 *  \param vertices Where the results of the computation are put, for use by the calling function.
 *  \param vertices_count The maximum number of vertices to use. Should be superior or equal to the
 *                       size of the arrays.
 *  \param measure The value of the variable measured by the meter.
 *  \param gauge_width The width of the meter
 *  \param gauge_height The height of the meter
 *  \param offset The offset to position the meter
 */
unsigned int RaceGUI::computeVerticesForMeter(core::vector2df position[], float threshold[], video::S3DVertex vertices[], unsigned int vertices_count,
                                     float measure, int gauge_width, int gauge_height, core::vector2df offset)
{
    //Nothing to draw ; we need at least three points to draw a triangle
    if (vertices_count <= 2 || measure < 0)
    {
        return 0;
    }

    unsigned int count=2;
    float f = 1.0f;

    for (unsigned int i=2 ; i < vertices_count ; i++)
    {
        count++;

        //Stop when we have found between which thresholds the measure is
        if (measure < threshold[i-2])
        {
            if (i-2 == 0)
            {
                f = measure/threshold[i-2];
            }
            else
            {
                f = (measure - threshold[i-3])/(threshold[i-2]-threshold[i-3]);
            }

            break;
        }
    }

    for (unsigned int i=0 ; i < count ; i++)
    {
        //if the measure don't fall in this segment, use the next predefined point
        if (i<count-1 || (count == vertices_count && f == 1.0f))
        {
            vertices[i].TCoords = core::vector2df(position[i].X, position[i].Y);
            vertices[i].Pos     = core::vector3df(offset.X+position[i].X*gauge_width,
                                  offset.Y-(1-position[i].Y)*gauge_height, 0);
        }
        //if the measure fall in this segment, compute the variable position
        else
        {
            //f : the proportion of the next point. 1-f : the proportion of the previous point
            vertices[i].TCoords = core::vector2df(position[i].X*(f)+position[i-1].X*(1.0f-f),
                                                  position[i].Y*(f)+position[i-1].Y*(1.0f-f));
            vertices[i].Pos = core::vector3df(offset.X+ ((position[i].X*(f)+position[i-1].X*(1.0f-f))*gauge_width),
                                              offset.Y-(((1-position[i].Y)*(f)+(1-position[i-1].Y)*(1.0f-f))*gauge_height),0);
        }
    }

    //the count is used in the drawing functions
    return count;
} //computeVerticesForMeter

//-----------------------------------------------------------------------------
/** Displays the lap of the kart.
 *  \param info Info object c
*/
void RaceGUI::drawLap(const AbstractKart* kart,
                      const core::recti &viewport,
                      const core::vector2df &scaling)
{
#ifndef SERVER_ONLY
    // Don't display laps if the kart has already finished the race.
    if (kart->hasFinishedRace()) return;

    World *world = World::getWorld();

    core::recti pos;
    
    pos.UpperLeftCorner.Y = viewport.UpperLeftCorner.Y + m_font_height;

    // If the time display in the top right is in this viewport,
    // move the lap/rank display down a little bit so that it is
    // displayed under the time.
    if (viewport.UpperLeftCorner.Y == 0 &&
        viewport.LowerRightCorner.X == (int)(irr_driver->getActualScreenSize().Width) &&
        !RaceManager::get()->getIfEmptyScreenSpaceExists()) 
    {
        pos.UpperLeftCorner.Y = irr_driver->getActualScreenSize().Height*12/100;
    }
    pos.LowerRightCorner.Y  = viewport.LowerRightCorner.Y+20;
    pos.UpperLeftCorner.X   = viewport.LowerRightCorner.X
                            - m_lap_width - 10;
    pos.LowerRightCorner.X  = viewport.LowerRightCorner.X;

    // Draw CTF / soccer scores with red score - blue score (score limit)
    CaptureTheFlag* ctf = dynamic_cast<CaptureTheFlag*>(World::getWorld());
    SoccerWorld* sw = dynamic_cast<SoccerWorld*>(World::getWorld());
    FreeForAll* ffa = dynamic_cast<FreeForAll*>(World::getWorld());

    static video::SColor color = video::SColor(255, 255, 255, 255);
    int hit_capture_limit =
        (RaceManager::get()->getHitCaptureLimit() != std::numeric_limits<int>::max()
         && RaceManager::get()->getHitCaptureLimit() != 0)
        ? RaceManager::get()->getHitCaptureLimit() : -1;
    int score_limit = sw && !RaceManager::get()->hasTimeTarget() ?
        RaceManager::get()->getMaxGoal() : ctf ? hit_capture_limit : -1;
    if (!ctf && ffa && hit_capture_limit != -1)
    {
        int icon_width = irr_driver->getActualScreenSize().Height/19;
        core::rect<s32> indicator_pos(viewport.LowerRightCorner.X - (icon_width+10),
                                    pos.UpperLeftCorner.Y,
                                    viewport.LowerRightCorner.X - 10,
                                    pos.UpperLeftCorner.Y + icon_width);
        core::rect<s32> source_rect(core::position2d<s32>(0,0),
                                                m_champion->getSize());
        draw2DImage(m_champion, indicator_pos, source_rect,
            NULL, NULL, true);

        gui::ScalableFont* font = GUIEngine::getHighresDigitFont();
        font->setBlackBorder(true);
        pos.UpperLeftCorner.X += 30;
        font->draw(StringUtils::toWString(hit_capture_limit).c_str(), pos, color);
        font->setBlackBorder(false);
        font->setScale(1.0f);
        return;
    }

    if (ctf || sw)
    {
        int red_score = ctf ? ctf->getRedScore() : sw->getScore(KART_TEAM_RED);
        int blue_score = ctf ? ctf->getBlueScore() : sw->getScore(KART_TEAM_BLUE);
        gui::ScalableFont* font = GUIEngine::getHighresDigitFont();
        font->setBlackBorder(true);
        font->setScale(1.0f);
        core::dimension2du d;
        if (score_limit != -1)
        {
             d = font->getDimension(
                (StringUtils::toWString(red_score) + L"-"
                + StringUtils::toWString(blue_score) + L"     "
                + StringUtils::toWString(score_limit)).c_str());
            pos.UpperLeftCorner.X -= d.Width / 2;
            int icon_width = irr_driver->getActualScreenSize().Height/19;
            core::rect<s32> indicator_pos(viewport.LowerRightCorner.X - (icon_width+10),
                                        pos.UpperLeftCorner.Y,
                                        viewport.LowerRightCorner.X - 10,
                                        pos.UpperLeftCorner.Y + icon_width);
            core::rect<s32> source_rect(core::position2d<s32>(0,0),
                                                    m_champion->getSize());
            draw2DImage(m_champion, indicator_pos, source_rect,
                NULL, NULL, true);
        }

        core::stringw text = StringUtils::toWString(red_score);
        font->draw(text, pos, video::SColor(255, 255, 0, 0));
        d = font->getDimension(text.c_str());
        pos += core::position2di(d.Width, 0);
        text = L"-";
        font->draw(text, pos, video::SColor(255, 255, 255, 255));
        d = font->getDimension(text.c_str());
        pos += core::position2di(d.Width, 0);
        text = StringUtils::toWString(blue_score);
        font->draw(text, pos, video::SColor(255, 0, 0, 255));
        pos += core::position2di(d.Width, 0);
        if (score_limit != -1)
        {
            text = L"     ";
            text += StringUtils::toWString(score_limit);
            font->draw(text, pos, video::SColor(255, 255, 255, 255));
        }
        font->setBlackBorder(false);
        return;
    }

    if (!world->raceHasLaps()) return;
    int lap = world->getFinishedLapsOfKart(kart->getWorldKartId());
    // Network race has larger lap than getNumLaps near finish line
    // due to waiting for final race result from server
    if (lap + 1> RaceManager::get()->getNumLaps())
        lap--;
    // don't display 'lap 0/..' at the start of a race
    if (lap < 0 ) return;

    // Display lap flag


    int icon_width = irr_driver->getActualScreenSize().Height/19;
    core::rect<s32> indicator_pos(viewport.LowerRightCorner.X - (icon_width+10),
                                  pos.UpperLeftCorner.Y,
                                  viewport.LowerRightCorner.X - 10,
                                  pos.UpperLeftCorner.Y + icon_width);
    core::rect<s32> source_rect(core::position2d<s32>(0,0),
                                               m_lap_flag->getSize());
    draw2DImage(m_lap_flag,indicator_pos,source_rect,
        NULL,NULL,true);

    pos.UpperLeftCorner.X -= icon_width;
    pos.LowerRightCorner.X -= icon_width;

    std::ostringstream out;
    out << lap + 1 << "/" << RaceManager::get()->getNumLaps();

    gui::ScalableFont* font = GUIEngine::getHighresDigitFont();
    font->setBlackBorder(true);
    font->draw(out.str().c_str(), pos, color);
    font->setBlackBorder(false);
    font->setScale(1.0f);
#endif
} // drawLap

void RaceGUI::drawHeadingLine(const AbstractKart* kart, float length)
{
#ifndef SERVER_ONLY
    if (kart == NULL  || kart->hasFinishedRace()) return;
    World* world = World::getWorld();
    if (world->isGoalPhase()) return ;
    // only show the line in soccer, FFA, and CTF
    if (RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_3_STRIKES ||
        RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_SOCCER ||
        RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_FREE_FOR_ALL ||
        RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_CAPTURE_THE_FLAG)
    {
        Vec3 kart_front_xyz = kart->getFrontXYZ();
        // heading direction
        float theta = kart->getHeading();
        Vec3 line_end_xyz = kart_front_xyz + Vec3(length*sinf(theta),0.0,length*cosf(theta));
        video::SColor line_color = video::SColor(255, 0, 255, 0);
        draw3DLine(kart_front_xyz.toIrrVector(), line_end_xyz.toIrrVector(), line_color);
        // visual skid direction
        theta += kart->getSkidding()->getVisualSkidRotation();
        line_end_xyz = kart_front_xyz + Vec3(length*sinf(theta),0.0,length*cosf(theta));
        line_color = video::SColor(255, 0, 100, 0);
        draw3DLine(kart_front_xyz.toIrrVector(), line_end_xyz.toIrrVector(), line_color);
    }
#endif
} // drawHeadingLine

void RaceGUI::drawBallLine(float length)
{
    SoccerWorld *soccer_world = dynamic_cast<SoccerWorld*>(World::getWorld());
    Vec3 ball_xyz = soccer_world->getBallPosition();
    float theta = soccer_world->getBallHeading();
    // three colors to better understand the distance
    Vec3 line_end_xyz = ball_xyz + Vec3(length*sinf(theta),0.0,length*cosf(theta));
    video::SColor line_color = video::SColor(228, 26, 28, 0);
    draw3DLine(ball_xyz.toIrrVector(), line_end_xyz.toIrrVector(), line_color);
    ball_xyz = line_end_xyz;
    line_end_xyz = ball_xyz + Vec3(length*sinf(theta),0.0,length*cosf(theta));
    line_color = video::SColor(255, 127, 0, 0);
    draw3DLine(ball_xyz.toIrrVector(), line_end_xyz.toIrrVector(), line_color);
    ball_xyz = line_end_xyz;
    line_end_xyz = ball_xyz + Vec3(length*sinf(theta),0.0,length*cosf(theta));
    line_color = video::SColor(255, 255, 51, 0);
    draw3DLine(ball_xyz.toIrrVector(), line_end_xyz.toIrrVector(), line_color);
} // drawBallLine

// Draw the numeric speedometer
void RaceGUI::drawNumericSpeed(const AbstractKart *kart,
                               const core::vector2df &offset,
                               int meter_width, int meter_height)
{
    static video::SColor color = video::SColor(255, 255, 255, 255);
    gui::ScalableFont* font = GUIEngine::getHighresDigitFont();

    core::recti pos;
    pos.LowerRightCorner = core::vector2di(int(offset.X + 0.65f*meter_width), int(offset.Y - 0.55f*meter_height));
    pos.UpperLeftCorner = core::vector2di(int(offset.X + 0.65f*meter_width), int(offset.Y - 0.55f*meter_height));

    std::ostringstream oss2;
    oss2 << std::fixed << std::setprecision(1) << kart->getSpeed()/KILOMETERS_PER_HOUR; //

    font->draw(oss2.str().c_str(), pos, color, true, true);

    /*font->setScale(0.4f);
    pos.LowerRightCorner = core::vector2di(int(offset.X + 0.65f*meter_width), int(offset.Y - 0.45f*meter_height));
    pos.UpperLeftCorner = core::vector2di(int(offset.X + 0.65f*meter_width), int(offset.Y - 0.45f*meter_height));

    if (RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_SOCCER)
    {   // show ball speed in soccer
        SoccerWorld *soccer_world = dynamic_cast<SoccerWorld*>(World::getWorld());
        std::ostringstream oss3;
        oss3 << "ball " << std::fixed << std::setprecision(1) << soccer_world->getBallVelocity();
        font->draw(oss3.str().c_str(), pos, color, true, true);
        // show own score
        KartTeam cur_team = soccer_world->getKartTeam(kart->getWorldKartId());
        KartTeam opp_team = (cur_team == KART_TEAM_BLUE ? KART_TEAM_RED : KART_TEAM_BLUE);
        std::vector<SoccerWorld::ScorerData> scorers = soccer_world->getScorers(cur_team);
        unsigned int goal=0, own_goal=0;
        for (unsigned int i = 0; i < scorers.size(); i++)
            if (scorers.at(i).m_player == kart->getController()->getName())
                goal++;
        scorers = soccer_world->getScorers(opp_team);
        for (unsigned int i = 0; i < scorers.size(); i++)
            if (scorers.at(i).m_player == kart->getController()->getName())
                own_goal++;
        pos.LowerRightCorner = core::vector2di(int(offset.X + 0.65f*meter_width), int(offset.Y - 0.40f*meter_height));
        pos.UpperLeftCorner = core::vector2di(int(offset.X + 0.65f*meter_width), int(offset.Y - 0.40f*meter_height));
        std::ostringstream oss4;
        oss4 << "goal " << goal << " - " << own_goal;
        font->draw(oss4.str().c_str(), pos, color, true, true);
    }
    else if (RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_NORMAL_RACE ||
             RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_TIME_TRIAL ||
             RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_FOLLOW_LEADER)
    {   // show the average kart speed
        if (!kart->hasFinishedRace())
        {
            m_final_speed_avg = 0.;
            m_speed_sum += std::abs(kart->getSpeed()); // /KILOMETERS_PER_HOUR
            m_speed_samples++;

            float avg_speed(m_speed_sum/m_speed_samples);
            std::ostringstream oss3;
            oss3 << std::fixed << std::setprecision(1) << avg_speed << "/" << std::setprecision(3) << avg_speed*World::getWorld()->getTime()/3600;
            font->draw(oss3.str().c_str(), pos, color, true, true);
        }
        else
        {
            if (m_final_speed_avg == 0.)
            {
                m_final_speed_avg = m_speed_sum/m_speed_samples;
                m_final_distance = m_final_speed_avg*World::getWorld()->getTime()/3600;
                m_speed_sum = 0.;
                m_speed_samples = 0;
            }

            std::ostringstream oss3;
            oss3 << std::fixed << std::setprecision(1) << m_final_speed_avg << "/" << std::setprecision(3) << m_final_distance;
            color = video::SColor(255, 0, 255, 0);
            font->draw(oss3.str().c_str(), pos, color, true, true);
            color = video::SColor(255, 255, 255, 255);
        }
    }*/
    // TODO: else show other useful information in CTF and FFA
    font->setScale(1.0f);
}

// Draw a more detailed numeric speedometer
void RaceGUI::drawNumericSpeed2(const AbstractKart *kart,
                               const core::vector2df &offset,
                               int meter_width, int meter_height)
{
    static video::SColor color = video::SColor(255, 255, 255, 255);
    static video::SColor color_red = video::SColor(255, 255, 0, 0);
    static video::SColor color_green = video::SColor(255, 0, 255, 0);
    gui::ScalableFont* font = GUIEngine::getHighresDigitFont();

    font->setScale(0.4f);
    core::recti pos;
    std::ostringstream oss;
    float duration = 0.0f;

    // current time
    pos.LowerRightCorner = core::vector2di(int(offset.X - 0.2f*meter_width), int(offset.Y - 2.0f*meter_height + 0.4f*m_font_height));
    pos.UpperLeftCorner  = core::vector2di(int(offset.X - 1.2f*meter_width), int(offset.Y - 2.0f*meter_height));
    oss << std::fixed << std::setprecision(3) << "Time : " << World::getWorld()->getTime() << " s";
    font->draw(oss.str().c_str(), pos, color, false, true);

    // current speed / max. speed
    pos.LowerRightCorner.Y += 0.4f*m_font_height;
    pos.UpperLeftCorner.Y += 0.4f*m_font_height;
    oss.str(""); oss.clear();
    oss << std::fixed << std::setprecision(3) << "Speed: " << kart->getSpeed() << " m/s"; // /KILOMETERS_PER_HOUR
    font->draw(oss.str().c_str(), pos, color, false, true);

    pos.LowerRightCorner.Y += 0.4f*m_font_height;
    pos.UpperLeftCorner.Y += 0.4f*m_font_height;
    oss.str(""); oss.clear();
    oss << std::fixed << std::setprecision(3) << "Max. : " << kart->getCurrentMaxSpeed() << " m/s"; // /KILOMETERS_PER_HOUR
    font->draw(oss.str().c_str(), pos, color, false, true);

    pos.LowerRightCorner.Y += 0.4f*m_font_height;
    pos.UpperLeftCorner.Y += 0.4f*m_font_height;
    oss.str(""); oss.clear();
    oss << std::fixed << std::setprecision(2) << "Nitro: " << kart->getEnergy();
    font->draw(oss.str().c_str(), pos, color, false, true);

    pos.LowerRightCorner.Y += 0.4f*m_font_height;
    pos.UpperLeftCorner.Y += 0.4f*m_font_height;
    oss.str(""); oss.clear();
    oss << "-----------------";
    font->draw(oss.str().c_str(), pos, color, false, true);

    pos.LowerRightCorner.Y += 0.4f*m_font_height;
    pos.UpperLeftCorner.Y += 0.4f*m_font_height;
    oss.str(""); oss.clear();
    duration = (float)kart->getSpeedIncreaseTicksLeft(MaxSpeed::MS_INCREASE_ZIPPER);
    if (duration > 0)
    {
        oss << std::fixed << std::setprecision(3) << "zipper : " << duration/100 << " s";
        font->draw(oss.str().c_str(), pos, color_green, false, true);
    }
    else
    {
        oss << "zipper : ";
        font->draw(oss.str().c_str(), pos, color, false, true);
    }

    pos.LowerRightCorner.Y += 0.4f*m_font_height;
    pos.UpperLeftCorner.Y += 0.4f*m_font_height;
    oss.str(""); oss.clear();
    duration = (float)kart->getSpeedIncreaseTicksLeft(MaxSpeed::MS_INCREASE_NITRO);
    if (duration > 0)
    {
        oss << std::fixed << std::setprecision(3) << "nitro  : " << duration/100 << " s";
        font->draw(oss.str().c_str(), pos, color_green, false, true);
    }
    else
    {
        oss << "nitro  : ";
        font->draw(oss.str().c_str(), pos, color, false, true);
    }

    pos.LowerRightCorner.Y += 0.4f*m_font_height;
    pos.UpperLeftCorner.Y += 0.4f*m_font_height;
    oss.str(""); oss.clear();
    duration = (float)kart->getSpeedIncreaseTicksLeft(MaxSpeed::MS_INCREASE_SKIDDING);
    if (duration > 0)
    {
        oss << std::fixed << std::setprecision(3) << "skid y : " << duration/100 << " s";
        font->draw(oss.str().c_str(), pos, color_green, false, true);
    }
    else
    {
        oss << "skid y : ";
        font->draw(oss.str().c_str(), pos, color, false, true);
    }

    pos.LowerRightCorner.Y += 0.4f*m_font_height;
    pos.UpperLeftCorner.Y += 0.4f*m_font_height;
    oss.str(""); oss.clear();
    duration = (float)kart->getSpeedIncreaseTicksLeft(MaxSpeed::MS_INCREASE_RED_SKIDDING);
    if (duration > 0)
    {
        oss << std::fixed << std::setprecision(3) << "skid r : " << duration/100 << " s";
        font->draw(oss.str().c_str(), pos, color_green, false, true);
    }
    else
    {
        oss << "skid r : ";
        font->draw(oss.str().c_str(), pos, color, false, true);
    }

    // show those related to item usage
    if (RaceManager::get()->getMinorMode() != RaceManager::MINOR_MODE_TIME_TRIAL)
    {
        pos.LowerRightCorner.Y += 0.4f*m_font_height;
        pos.UpperLeftCorner.Y += 0.4f*m_font_height;
        oss.str(""); oss.clear();
        duration = (float)kart->getSpeedIncreaseTicksLeft(MaxSpeed::MS_INCREASE_SLIPSTREAM);
        if (duration > 0)
        {
            oss << std::fixed << std::setprecision(3) << "slip st: " << duration/100 << " s";
            font->draw(oss.str().c_str(), pos, color_green, false, true);
        }
        else
        {
            oss << "slip st: ";
            font->draw(oss.str().c_str(), pos, color, false, true);
        }

        pos.LowerRightCorner.Y += 0.4f*m_font_height;
        pos.UpperLeftCorner.Y += 0.4f*m_font_height;
        oss.str(""); oss.clear();
        duration = (float)kart->getSpeedIncreaseTicksLeft(MaxSpeed::MS_INCREASE_RUBBER);
        if (duration > 0)
        {
            oss << std::fixed << std::setprecision(3) << "rubber : " << duration/100 << " s";
            font->draw(oss.str().c_str(), pos, color_green, false, true);
        }
        else
        {
            oss << "rubber : ";
            font->draw(oss.str().c_str(), pos, color, false, true);
        }
    }

    pos.LowerRightCorner.Y += 0.4f*m_font_height;
    pos.UpperLeftCorner.Y += 0.4f*m_font_height;
    oss.str(""); oss.clear();
    duration = (float)kart->getSpeedDecreaseTicksLeft(MaxSpeed::MS_DECREASE_TERRAIN);
    if (duration > 0)
    {
        oss << std::fixed << std::setprecision(3) << "terrain: " << duration/100 << " s"
            << " [" << kart->getMaxSpeed()->getSlowdownFraction(MaxSpeed::MS_DECREASE_TERRAIN) << "]";
        font->draw(oss.str().c_str(), pos, color_red, false, true);
    }
    else
    {
        oss << "terrain: ";
        font->draw(oss.str().c_str(), pos, color, false, true);
    }

    // show those related to item usage
    if (RaceManager::get()->getMinorMode() != RaceManager::MINOR_MODE_TIME_TRIAL)
    {
        pos.LowerRightCorner.Y += 0.4f*m_font_height;
        pos.UpperLeftCorner.Y += 0.4f*m_font_height;
        oss.str(""); oss.clear();
        duration = (float)kart->getSpeedDecreaseTicksLeft(MaxSpeed::MS_DECREASE_BUBBLE);
        if (duration > 0)
        {
            oss << std::fixed << std::setprecision(3) << "bubble : " << duration/100 << " s"
                << " [" << kart->getMaxSpeed()->getSlowdownFraction(MaxSpeed::MS_DECREASE_BUBBLE) << "]";
            font->draw(oss.str().c_str(), pos, color_red, false, true);
        }
        else
        {
            oss << "bubble : ";
            font->draw(oss.str().c_str(), pos, color, false, true);
        }

        pos.LowerRightCorner.Y += 0.4f*m_font_height;
        pos.UpperLeftCorner.Y += 0.4f*m_font_height;
        oss.str(""); oss.clear();
        duration = (float)kart->getSpeedDecreaseTicksLeft(MaxSpeed::MS_DECREASE_SQUASH);
        if (duration > 0)
        {
            oss << std::fixed << std::setprecision(3) << "squash : " << duration/100 << " s"
                << " [" << kart->getMaxSpeed()->getSlowdownFraction(MaxSpeed::MS_DECREASE_SQUASH) << "]";
            font->draw(oss.str().c_str(), pos, color_red, false, true);
        }
        else
        {
            oss << "squash : ";
            font->draw(oss.str().c_str(), pos, color, false, true);
        }
    }

    pos.LowerRightCorner.Y += 6.0*0.4f*m_font_height;
    pos.UpperLeftCorner.Y += 0.4f*m_font_height;
    oss.str(""); oss.clear();
    oss << "-----------------" << std::endl
        << "Action    :" << (uint16_t) kart->getControls().getButtonsCompressed() << std::endl
        << "Steer     :" << kart->getControls().getSteer() << std::endl
        << "Accel     :" << kart->getControls().getAccel() << std::endl
        << "Skid      :" << kart->getSkidding()->getSkidFactor() << std::endl
        << "Skid state:" << (int)kart->getSkidding()->getSkidState() << std::endl;
    font->draw(oss.str().c_str(), pos, color, false, true);

    font->setScale(1.0f);
}

// draw a Hub to show the speed
void RaceGUI::drawHubSpeed(const AbstractKart *kart,
                           const core::vector2df &offset,
                           int hub_width,int hub_height)
{

    if (RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_SOCCER)
    {
        SoccerWorld *soccer_world = dynamic_cast<SoccerWorld*>(World::getWorld());
        static video::SColor color = video::SColor(255, 255, 255, 255);
        gui::ScalableFont* font = GUIEngine::getHighresDigitFont();
        core::recti pos;
        font->setScale(0.4f);
        // draw the ball speed
        pos.LowerRightCorner = core::vector2di(int(offset.X - 0.5f*hub_width), int(offset.Y));
        pos.UpperLeftCorner  = core::vector2di(int(offset.X + 0.5f*hub_width), int(offset.Y - 0.5f*hub_height));
        std::ostringstream oss;
        oss << "Ball " << std::fixed << std::setprecision(1) << soccer_world->getBallVelocity();
        font->draw(oss.str().c_str(), pos, color, true, true);
        // draw the kart speed
        pos.LowerRightCorner = core::vector2di(int(offset.X - 0.5f*hub_width), int(offset.Y - 0.5f*hub_height));
        pos.UpperLeftCorner  = core::vector2di(int(offset.X + 0.5f*hub_width), int(offset.Y - 1.0f*hub_height));
        std::ostringstream oss1;
        oss1 << "Kart " << std::fixed << std::setprecision(1) << kart->getSpeed();
        font->draw(oss1.str().c_str(), pos, color, true, true);
        font->setScale(1.0f);
    }
}