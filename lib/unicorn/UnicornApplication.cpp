/*
   Copyright 2005-2009 Last.fm Ltd. 
      - Primarily authored by Max Howell, Jono Cole and Doug Mansell

   This file is part of the Last.fm Desktop Application Suite.

   lastfm-desktop is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   lastfm-desktop is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with lastfm-desktop.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef __APPLE__
    // first to prevent compilation errors with Qt 4.5.0-beta1
    #include <Carbon/Carbon.h>
    #include <ApplicationServices/ApplicationServices.h>
    static pascal OSErr appleEventHandler( const AppleEvent*, AppleEvent*, long );
    #include <QMainWindow>
    extern void qt_mac_set_menubar_icons( bool );
#endif

#include "UnicornApplication.h"
#include "QMessageBoxBuilder.h"
#include "UnicornCoreApplication.h"
#include "widgets/LoginDialog.h"
#include "SignalBlocker.h"
#include "UnicornSettings.h"
#include <lastfm/misc.h>
#include <lastfm/User>
#include <lastfm/ws.h>
#include <lastfm/XmlQuery>
#include <QDir>
#include <QDebug>
#include <QLocale>
#include <QTranslator>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QDebug>

unicorn::Application::Application( int& argc, char** argv ) throw( StubbornUserException )
                    : QApplication( argc, argv ),
                      m_logoutAtQuit( false ),
                      m_signingIn( true )
{
#ifdef Q_WS_MAC
    qt_mac_set_menubar_icons( false );
#endif    
    
    CoreApplication::init();

#ifdef __APPLE__
    AEEventHandlerUPP h = NewAEEventHandlerUPP( appleEventHandler );
    AEInstallEventHandler( kCoreEventClass, kAEReopenApplication, h, 0, false );
#endif

    #ifdef Q_WS_MAC
        #define CSS_PATH "/../Resources/"
    #else
        #define CSS_PATH "/"
    #endif
    if( styleSheet().isEmpty()) {
        QString cssFileName = QFileInfo(applicationFilePath()).baseName() + ".css";
        QFile stylesheetFile( applicationDirPath() + CSS_PATH + cssFileName );
        stylesheetFile.open( QIODevice::ReadOnly );
        QString stylesheet( stylesheetFile.readAll() );
        setStyleSheet( stylesheet );
    }

    translate();

    connect( &m_bus, SIGNAL( signingInQuery( QString )), SLOT( onSigningInQuery( QString )));
    connect( &m_bus, SIGNAL( sessionQuery( QString )), SLOT( onBusSessionQuery( QString )));
    connect( &m_bus, SIGNAL( sessionChanged( Session )), SLOT( onBusSessionChanged( Session )));

    m_bus.board();

    if( !styleSheet().isEmpty() ) {
        QString cssPath = QUrl( styleSheet() ).toLocalFile();
        cssPath = QDir::currentPath() + cssPath;
        QFile cssFile( cssPath );
        cssFile.open( QIODevice::ReadOnly );
        m_styleSheet = cssFile.readAll();
        cssFile.close();
    }

#ifdef __APPLE__
    setQuitOnLastWindowClosed( false );
#endif
    QTimer::singleShot( 0, this, SLOT( init()));

    if( m_bus.isSigningIn() ) {
        if( !SignalBlocker( &m_bus, SIGNAL( sessionChanged(Session)), -1 ).start())
        {
            quit();
        }
    } else {
        
        Session busSession = m_bus.getSession();
       
        if( busSession.isValid() )
            m_currentSession = busSession;
    }
    
    if( !m_currentSession.isValid() )
    {
        m_signingIn = true;
        LoginDialog d( m_currentSession.username() );
        connect( &m_bus, SIGNAL( signingInQuery( QString)), &d, SLOT( raise()));
        if (d.exec() == QDialog::Accepted)
        {
            m_currentSession = d.session();
            QByteArray* ba = new QByteArray("");
            QDataStream ds( ba, QIODevice::WriteOnly | QIODevice::Truncate);
            ds << QByteArray( "SESSIONCHANGED" );
            ds << currentSession();
            m_bus.sendMessage( *ba );
            delete ba;
        }
        else
        {
            quit();
        }
    }
    m_signingIn = false;
    connect( AuthenticatedUser().getInfo(), SIGNAL(finished()), SLOT(onUserGotInfo()) );
}


void 
unicorn::Application::init()
{
}


void
unicorn::Application::translate()
{
#ifdef NDEBUG
    QString const iso3166 = QLocale().name().right( 2 ).toLower();

#ifdef Q_WS_MAC
    QDir const d = lastfm::dir::bundle().filePath( "Contents/Resources/qm" );
#else
    QDir const d = qApp->applicationDirPath() + "/i18n";
#endif

    //TODO need a unicorn/core/etc. translation, plus policy of no translations elsewhere or something!
    QTranslator* t1 = new QTranslator;
    t1->load( d.filePath( "lastfm_" + iso3166 ) );

    QTranslator* t2 = new QTranslator;
    t2->load( d.filePath( "qt_" + iso3166 ) );

    installTranslator( t1 );
    installTranslator( t2 );
#endif
}


unicorn::Application::~Application()
{
    // we do this here, rather than when the setting is changed because if we 
    // did it then the user would be unable to change their mind
    /*
    if (Settings().logOutOnExit() || m_logoutAtQuit)
    {
        GlobalSettings s;
        s.remove( "SessionKey" );
        s.remove( "Password" );
    }*/
}


void
unicorn::Application::onUserGotInfo()
{
    QNetworkReply* reply = (QNetworkReply*)sender();
    
    try
    {
        XmlQuery lfm = lastfm::ws::parse( reply );
        const char* key = UserSettings::subscriptionKey();
        bool const value = lfm["user"]["subscriber"].text().toInt() == 1;
        UserSettings().setValue( key, value );
    }
    catch (std::runtime_error& e)
    {
        qWarning() << e.what();
    }


    emit userGotInfo( reply );
}


void 
unicorn::Application::onSigningInQuery( const QString& uuid )
{
    qDebug() << "Are we signing in? " << m_signingIn;
    if( m_signingIn )
        m_bus.sendQueryResponse( uuid, "TRUE" );
    else
        m_bus.sendQueryResponse( uuid, "FALSE" );
}


void 
unicorn::Application::onBusSessionQuery( const QString& uuid )
{
    QByteArray ba;
    QDataStream s( &ba, QIODevice::WriteOnly );
    s << currentSession();
    m_bus.sendQueryResponse( uuid, ba );
}


void 
unicorn::Application::onBusSessionChanged( const Session& session )
{
    changeSession( session );
}

void 
unicorn::Application::changeSession( const Session& newSession )
{
    Session oldSession = currentSession();
    m_currentSession = newSession;
    emit sessionChanged( currentSession(), oldSession );
}


#ifdef __APPLE__
static pascal OSErr appleEventHandler( const AppleEvent* e, AppleEvent*, long )
{
    OSType id = typeWildCard;
    AEGetAttributePtr( e, keyEventIDAttr, typeType, 0, &id, sizeof(id), 0 );
    
    switch (id)
    {
        case kAEQuitApplication:
            qApp->quit();
            return noErr;

        case kAEReopenApplication:
        {
            foreach (QWidget* w, qApp->topLevelWidgets())
                if (qobject_cast<QMainWindow*>(w))
                    w->show(), w->raise();
            return noErr;
        }

        default:
            return unimpErr;
    }
}
#endif
