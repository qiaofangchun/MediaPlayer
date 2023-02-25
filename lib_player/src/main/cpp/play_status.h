//
// Created by admin on 2022/11/25.
//

#ifndef HOLA_PLAY_STATUS_H
#define HOLA_PLAY_STATUS_H

/*enum PlayStatus {
    STATUS_INIT, STATUS_PREPARE,
    STATUS_STOP, STATUS_PLAYING,
    STATUS_PAUSE, STATUS_SEEK,
};*/

class PlayStatus {
public:
    bool isExit = false;
    bool isLoading = false;
    bool isPause = false;
    bool isSeek = false;

public:
    PlayStatus();
};


#endif //HOLA_PLAY_STATUS_H
