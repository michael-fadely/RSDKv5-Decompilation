/**
    MPEG1 Decode Library for Dreamcast - Version 0.8 (2023/09/19)
    Originally ported by Tashi (aka Twada)
    Further edits done by Ian Robinson and Andy Barajas

    Overview:
    This library facilitates the playback of MPEG1 videos on the Sega Dreamcast console.
    It supports monaural audio and allows specifying a cancel button during playback.

    Key Features:
    - Video Playback: MPEG1 video playback.
    - Audio Support: Mono audio playback. Stereo videos will play only the left channel.
    - Cancel Button: Allows specifying a controller button combination to cancel playback.
    - Recommended Resolutions:
      - 4:3 Aspect Ratio: 320x240 pixels, Mono audio at 80kbits.
      - 16:9 Aspect Ratio: 368x208 pixels, Mono audio at 80kbits.

    To create compatible MPEG1 videos, use the following ffmpeg command:

    ffmpeg -i input.mp4 -vf "scale=320:240" -b:v 742k -minrate 742k -maxrate 742k -bufsize 742k -ac 1 -ar 32000 -c:a mp2 -b:a 64k -f mpeg output.mpg
 */

#ifndef _MPEG_H_INCLUDED_
#define _MPEG_H_INCLUDED_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

typedef struct mpeg_player_t mpeg_player_t;

/** \brief   Create an MPEG player instance.
    \ingroup mpeg_playback

    This function initializes an MPEG player for video and audio playback. 
    It allocates memory for the mpeg_player_t structure, initializes the MPEG 
    decoder with the given filename, and sets up the graphics and audio systems 
    for playback.

    \param  filename        The filename of the MPEG file to be played. Must not be NULL.
    \return                 A pointer to an initialized mpeg_player_t structure, 
                            or NULL if initialization fails at any stage.
*/
mpeg_player_t *mpeg_player_create(const char *filename);

/** \brief   Create an MPEG player instance from memory.
    \ingroup mpeg_playback

    This function initializes an MPEG player for video and audio playback 
    using MPEG data stored in memory. It allocates memory for the mpeg_player_t 
    structure, initializes the MPEG decoder with the provided memory buffer, 
    and sets up the graphics and audio systems for playback.

    \param  memory          The pointer to the MPEG data in memory. Must not be NULL.
    \param  length          The size of the MPEG data in bytes.
    \return                 A pointer to an initialized mpeg_player_t structure, 
                            or NULL if initialization fails at any stage.
*/
mpeg_player_t *mpeg_player_create_memory(unsigned char *memory, const size_t length);

/**
    \brief   Retrieves the loop status of the MPEG player.
    \ingroup mpeg_playback

    This function checks whether the MPEG player is set to loop playback.

    \param   player  The MPEG player instance.
    \return          An integer representing the loop status (non-zero for loop).
 */
int mpeg_player_get_loop(mpeg_player_t *player);

/**
    \brief   Sets the loop status of the MPEG player.
    \ingroup mpeg_playback

    This function configures the MPEG player to either loop or not loop playback
    based on the provided loop parameter.

    \param   player  The MPEG player instance to configure.
    \param   loop    An integer indicating the desired loop status (non-zero for loop).
 */
void mpeg_player_set_loop(mpeg_player_t *player, int loop);

/** \brief   Destroy an MPEG player instance.
    \ingroup mpeg_playback

    This function releases all resources associated with an MPEG player. It 
    frees the memory allocated for the mpeg_player_t structure and any internal 
    components such as the MPEG decoder, texture, and sound buffer. If a valid 
    sound handle exists, it is also destroyed.

    \param  player          The pointer to the mpeg_player_t structure to be destroyed. 
                            If NULL, the function does nothing.
*/
void mpeg_player_destroy(mpeg_player_t *player);

/** \brief   Run one frame of an MPEG video using an MPEG player.
    \ingroup mpeg_playback

    This function processes/plays one frame MPEG video using the specified 
    MPEG player instance. If the video is finished, it is indicated through done param.

    \param  player          The MPEG player instance used for playback. Must be initialized.
    \param  done            output parameter storing if playback is complete
    \return                 n/a
*/
void mpeg_process(mpeg_player_t *player, int *done);
#ifdef __cplusplus
}
#endif

#endif