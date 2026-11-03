(function () {
    const win = new WebviewWidget({
        title: "QuickJS App",
        width: 1400,
        height: 1000,
        center: true,
        noBorder: true,
        transparentBackground: true,
    }, (success) => {
        if (!success)
            return;
        win.setName('test-app');
        win.setVirtualHostNameForResources("test.app", "/html/app");
        win.navigate(win.getResourceUrl("/window/macos.html"));
    });

    win.setWebMessageReceivedCallback((msg) => {
        console.info('res ipc', msg)
    });

    console.info("应用启动");
    let exitCode = App.run();
    console.log("应用退出:", exitCode);

    return { a: 1 };
})();
