package main

import (
    "encoding/json"
    "fmt"
    "io"
    "net/http"
)

func (a *webApp) serveFactoryAPI(w http.ResponseWriter, r *http.Request) bool {
    if a.config.Transport != "native" || a.config.Section != "cpe1" || r.URL.Path != "/native" { return false }
    payload := []byte(`{"action":"status"}`)
    if r.Method == http.MethodPost {
        var err error
        payload, err = io.ReadAll(r.Body)
        if err != nil { http.Error(w, err.Error(), 400); return true }
    }
    output, err := a.backend.Execute(r.Context(), string(payload))
    w.Header().Set("Content-Type", "application/json; charset=utf-8")
    w.Header().Set("Cache-Control", "no-store")
    if err != nil {
        _ = json.NewEncoder(w).Encode(map[string]any{"ok":false,"error":fmt.Sprint(err)+": "+output})
    } else { _, _ = io.WriteString(w, output) }
    return true
}
