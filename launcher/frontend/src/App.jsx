import React, { useCallback, useEffect, useState } from "react";
import { GetAppState } from "../wailsjs/go/main/App";
import Background from "./components/Background";
import ImportScreen from "./screens/ImportScreen";
import ProgressScreen from "./screens/ProgressScreen";
import EulaScreen from "./screens/EulaScreen";
import LaunchScreen from "./screens/LaunchScreen";
import GameOptionsScreen from "./screens/GameOptionsScreen";
import DedicatedScreen from "./screens/DedicatedScreen";
import CreditsScreen from "./screens/CreditsScreen";
import "./styles/global.css";
import "./styles/animations.css";

function App() {
  const [screen, setScreen] = useState("loading");
  const [appState, setAppState] = useState({});
  const [importError, setImportError] = useState(null);

  const refreshState = useCallback(() => {
    return GetAppState().then((state) => {
      setAppState(state);
      return state;
    });
  }, []);

  useEffect(() => {
    refreshState().then((state) => {
      // Start at import screen if assets not imported, otherwise main screen.
      setScreen(state.assetsReady ? "launch" : "import");
    });
  }, [refreshState]);

  const handleBackToLaunch = useCallback(() => {
    refreshState().then(() => setScreen("launch"));
  }, [refreshState]);

  const handleImportComplete = useCallback(() => {
    refreshState().then(() => setScreen("launch"));
  }, [refreshState]);

  return (
    <>
      <Background />
      <div
        style={{
          flex: 1,
          display: "flex",
          flexDirection: "column",
          minHeight: 0,
          overflow: "hidden",
        }}
      >
        {screen === "loading" && (
          <div
            style={{
              flex: 1,
              display: "flex",
              alignItems: "center",
              justifyContent: "center",
            }}
          >
            <div className="spinner" />
          </div>
        )}

        {screen === "launch" && (
          <LaunchScreen
            appState={appState}
            onGameOptions={() => setScreen("gameoptions")}
            onDedicated={() => setScreen("dedicated")}
            onImportAssets={() => setScreen("import")}
            onCredits={() => setScreen("credits")}
          />
        )}

        {screen === "gameoptions" && (
          <GameOptionsScreen
            assetsReady={appState.assetsReady}
            onBack={handleBackToLaunch}
          />
        )}

        {screen === "dedicated" && (
          <DedicatedScreen
            assetsReady={appState.assetsReady}
            onBack={handleBackToLaunch}
          />
        )}

        {(screen === "import" || screen === "import-after-eula") && (
          <ImportScreen
            onImportComplete={() => { setImportError(null); setScreen("progress"); }}
            onBack={handleBackToLaunch}
            onEula={() => setScreen("eula")}
            importError={importError}
            autoDownload={screen === "import-after-eula"}
          />
        )}

        {screen === "eula" && (
          <EulaScreen
            onAccept={() => setScreen("import-after-eula")}
            onDecline={() => setScreen("import")}
          />
        )}

        {screen === "progress" && (
          <ProgressScreen
            onComplete={handleImportComplete}
            onError={(errMsg) => { setImportError(errMsg); setScreen("import"); }}
          />
        )}

        {screen === "credits" && <CreditsScreen onBack={handleBackToLaunch} />}
      </div>
    </>
  );
}

export default App;
