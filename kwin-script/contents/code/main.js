function isFloatingMedia(window) {
    if (!window)
        return false;

    // Plasma 6: resourceClass genelde ikili adıyla eşleşir
    if (window.resourceClass === "FloatingMediaApp")
        return true;

    // Bazı oturumlarda desktopFileName / resourceName farklılık gösterebilir
    if (window.resourceName === "FloatingMediaApp")
        return true;

    if (typeof window.desktopFileName === "string"
        && window.desktopFileName.indexOf("floatingmedia") !== -1)
        return true;

    return false;
}

function setFloatingMediaAbove(window) {
    if (!isFloatingMedia(window))
        return;

    try {
        window.keepAbove = true;
        if (typeof workspace.raiseWindow === "function")
            workspace.raiseWindow(window);
        print("FloatingMedia keepAbove uygulandı.");
    } catch (e) {
        print("FloatingMedia keepAbove hatası: " + e);
    }
}

workspace.windowAdded.connect(setFloatingMediaAbove);

var windows = workspace.windowList();
for (var i = 0; i < windows.length; i++)
    setFloatingMediaAbove(windows[i]);
