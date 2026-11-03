#include "AppInfo.h"
#include "common/Logger.hpp"
#include "JSBinding/JSBindings.h"

std::string appPath( char *argv0 ) {
    std::filesystem::path exe = std::filesystem::absolute( argv0 );
    if ( !exe.parent_path().empty() ) {
        return exe.parent_path().u8string();
    }
    return "";
}

int main( int argc, char *argv[] ) {
#if APP_BUILD_RELEASE == 0
    system( "chcp 65001 >nul" );
#endif
    Logger::getInstance().setLevel( Logger::Level::LOG );
    Logger::getInstance().setOutputToFile(
        Logger::FileMode::SINGLE_FILE, appPath( argv[0] ) + "/log.txt" );

    std::string scriptPath = "/backend/main.js";
    std::string source = utils::readResource( scriptPath );
    auto ret = webviewjs::exec( source, scriptPath, argc, argv );
    LOG_INFO << "exit:" << ret;

    return 0;
}
