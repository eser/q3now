import React, { useCallback, useEffect, useState } from "react";
import { GetAppState } from "../wailsjs/go/main/App";
import Background from "./components/Background";
import ImportScreen from "./screens/ImportScreen";
import ProgressScreen from "./screens/ProgressScreen";
import LaunchScreen from "./screens/LaunchScreen";
import GameOptionsScreen from "./screens/GameOptionsScreen";
import DedicatedScreen from "./screens/DedicatedScreen";
import CreditsScreen from "./screens/CreditsScreen";
import "./styles/global.css";
import "./styles/animations.css";

function App() {
  const [screen, setScreen] = useState("loading");
  const [appState, setAppState] = useState({});

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

        {screen === "import" && (
          <ImportScreen
            onImportComplete={() => setScreen("progress")}
            onBack={handleBackToLaunch}
          />
        )}

        {screen === "progress" && (
          <ProgressScreen
            onComplete={handleImportComplete}
            onError={() => setScreen("import")}
          />
        )}

        {screen === "credits" && <CreditsScreen onBack={handleBackToLaunch} />}
      </div>
    </>
  );
}

export default App;
