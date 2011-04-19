#ifndef STATIONPANELWIDGET_H
#define STATIONPANELWIDGET_H

#include <QWidget>

#include <lastfm/RadioStation>
#include "lib/unicorn/UnicornSession.h"

namespace Ui {
    class StationPanelWidget;
}

class StationPanelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StationPanelWidget(QWidget *parent = 0);
    ~StationPanelWidget();

private slots:
    void onTuningIn( const RadioStation& station );

    void onRecentClicked();
    void onFriendsClicked();
    void onNeighboursClicked();
    void onArtistsClicked();
    void onTagsClicked();

    void onGotRecentStations();
    void onGotFriends();
    void onGotNeighbours();
    void onGotArtists();
    void onGotTags();

    void onFriendsSortClicked();

    void onGetTasteometerCompare();

    void onSessionChange( unicorn::Session* newSession );

private:
    Ui::StationPanelWidget *ui;

    class FriendsSortFilterProxyModel* m_friendsProxyModel;
};

#endif // STATIONPANELWIDGET_H