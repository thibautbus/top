# The developer's offline answers never touch the player's files: --achievements-login with them writes neither
# achievements.txt nor achievements=on; and (with the user's Ages in ORACLES_ROM_DIR, skipped without) a game in a window
# replaying mods-claw-game.route with them keeps its unlock not acknowledged in the answers' directory, never beside the
# settings, leaves the account as it was, and suspends the achievements once the claw game's mod takes the rupees.
#   cmake -D LAUNCHER=... -D WORK=... -D SOURCE=... -P achievements_offline.cmake
file(REMOVE_RECURSE ${WORK})
set(home ${WORK}/home)
set(answers ${WORK}/answers)
file(MAKE_DIRECTORY ${home} ${answers})
set(account "user=RealPlayer\ntoken=REALTOKEN\n")
file(WRITE ${home}/achievements.txt "${account}")
file(WRITE ${home}/settings.txt "update_check=off\n")
file(WRITE ${answers}/login2.json "{\"Success\":true,\"User\":\"offline\",\"Token\":\"offline\",\"Score\":0,\"SoftcoreScore\":0,\"Messages\":0}")
file(WRITE ${answers}/startsession.json "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[],\"ServerNow\":1700000000}")
file(WRITE ${answers}/ping.json "{\"Success\":true}")
file(WRITE ${WORK}/password.txt "a password\n")

function(expect_untouched what)
    file(READ ${home}/achievements.txt now)
    if(NOT now STREQUAL account)
        message(FATAL_ERROR "${what}: achievements.txt changed: ${now}")
    endif()
    file(READ ${home}/settings.txt settings)
    if(settings MATCHES "achievements=on")
        message(FATAL_ERROR "${what}: achievements=on written")
    endif()
    if(EXISTS ${home}/achievements-pending.txt OR EXISTS ${home}/achievement-badges)
        message(FATAL_ERROR "${what}: an unlock or a badge kept beside the settings")
    endif()
endfunction()

execute_process(COMMAND ${CMAKE_COMMAND} -E env ORACLES_SETTINGS_DIR=${home}/ ${LAUNCHER} --achievements-login Someone --achievements-offline ${answers}
                INPUT_FILE ${WORK}/password.txt RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0 OR NOT err MATCHES "offline answers: nothing kept")
    message(FATAL_ERROR "the offline login: ${status}\n${err}")
endif()
expect_untouched("the offline login")

# Ages (USA), found by its SHA-1 as the routes find it.
set(rom "")
if(DEFINED ENV{ORACLES_ROM_DIR})
    file(GLOB candidates "$ENV{ORACLES_ROM_DIR}/*.gbc")
    foreach(candidate ${candidates})
        file(SHA1 "${candidate}" sha1)
        if(sha1 STREQUAL "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4")
            set(rom "${candidate}")
        endif()
    endforeach()
endif()
if(NOT rom)
    message(STATUS "the game in a window skipped: no Oracle of Ages (USA) in ORACLES_ROM_DIR")
    return()
endif()
# One achievement on the hardware's own HRAM, true from the start: unlocked after 600 frames, its award unanswered.
file(WRITE ${answers}/achievementsets.json
     "{\"Success\":true,\"GameId\":710,\"Title\":\"Offline test\",\"ConsoleId\":6,\"ImageIconUrl\":\"\",\"RichPresenceGameId\":710,\"RichPresencePatch\":\"\","
     "\"Sets\":[{\"AchievementSetId\":1,\"GameId\":710,\"Title\":null,\"Type\":\"core\",\"ImageIconUrl\":\"\",\"Achievements\":["
     "{\"ID\":5,\"Title\":\"Six hundred frames\",\"Description\":\"\",\"Flags\":3,\"Points\":1,\"MemAddr\":\"0xHff80>=0.600.\",\"Author\":\"t\","
     "\"BadgeName\":\"00005\",\"Created\":1,\"Modified\":1}],\"Leaderboards\":[]}]}")
file(GLOB route_files ${SOURCE}/routes/ages/mods-claw-game.route*)
file(COPY ${route_files} DESTINATION ${WORK})
execute_process(COMMAND ${CMAKE_COMMAND} -E env ORACLES_SETTINGS_DIR=${home}/ SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
                        ${LAUNCHER} --rom ${rom} --play ${WORK}/mods-claw-game.route --frames 3760 --mute --vsync off
                        --mods ${SOURCE}/mods/claw-game --mods ${SOURCE}/mods/fortune-teller --achievements-offline ${answers}
                WORKING_DIRECTORY ${SOURCE} RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the game: ${status}\n${err}")
endif()
expect_untouched("the game")
if(NOT err MATCHES "unlocked \"Six hundred frames\"")
    message(FATAL_ERROR "the game: no unlock\n${err}")
endif()
file(READ ${answers}/achievements-pending.txt pending)
if(NOT pending MATCHES "offline c4639cc61c049e5a085526bb6cac03bb 5 ")
    message(FATAL_ERROR "the game: the unlock not kept in the answers' directory: ${pending}")
endif()
if(NOT err MATCHES "suspended for this game: Achievements are paused until the game starts again: a mod changed the game\\.")
    message(FATAL_ERROR "the game: the mod's payment did not suspend the achievements\n${err}")
endif()
message(STATUS "offline answers: the player's files untouched, the unlock kept beside the answers, suspended by the mod")
