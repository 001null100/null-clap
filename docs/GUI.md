# GUI boundary

null-clap intentionally has no JUCE dependency. The framework exposes CLAP GUI hosting through the abstract `GuiDelegate` interface and leaves rendering/window creation to the consuming plug-in.

This gives us a clean split:

```text
CLAP host window
      |
nullclap::Plugin
      |
GuiDelegate
      |
JUCE / native / webview adapter in plug-in repo
      |
actual editor components
```

## Why not put JUCE in the framework?

The reusable concern is CLAP window negotiation, not a specific drawing toolkit. Keeping JUCE outside null-clap prevents its plug-in wrappers, audio abstractions and release cadence from becoming framework dependencies.

Motion Engine can still use JUCE Components. It will provide a small adapter implementing `GuiDelegate` and attach its top-level JUCE view to the CLAP parent window.

## Delegate lifecycle

A delegate must handle:

- supported window API query
- create/destroy
- show/hide
- scale
- size and optional resize hints
- parent attachment
- optional transient/floating behavior

`Plugin::implementsGui()` returns true only when a delegate has been installed.

## Parameters from the editor

Do not write `ParameterStore` atomics directly from controls. Use the gesture API so Bitwig receives proper host events:

```cpp
beginParameterGesture(id);
setParameterFromGui(id, plainValue);
endParameterGesture(id);
```

The UI may read `parameters().value(id)` to reflect host automation. GUI repaint/polling frequency is an application decision.
