from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def _function_body(text: str, signature: str) -> str:
    tail = text.split(signature, 1)[1]
    depth = 0
    started = False
    body = []
    for char in tail:
        if char == "{":
            depth += 1
            started = True
        elif char == "}":
            depth -= 1
            if started and depth == 0:
                return "".join(body)
        if started:
            body.append(char)
    raise AssertionError(f"unterminated function: {signature}")


def test_quick_resume_does_not_reset_gameplay_core() -> None:
    text = (ROOT / "gba/src/quick_game_scene.cpp").read_text()
    resume = _function_body(text, "void QuickGameScene::resume_presentation()")
    assert "_game.reset()" not in resume
    assert "_background" in resume


def test_construction_resume_does_not_restart_core() -> None:
    text = (ROOT / "gba/src/tower_construction_scene.cpp").read_text()
    resume = _function_body(text, "void TowerConstructionScene::resume_presentation()")
    assert "_construction.start(" not in resume
    assert "_background" in resume


def test_temporary_new_game_menu_is_removed_from_shell() -> None:
    header = (ROOT / "gba/include/tb/ui_shell.h").read_text()
    source = (ROOT / "gba/src/ui_shell.cpp").read_text()
    assert "_show_new_game_menu" not in header
    assert "UiScene::NewGameMenu" not in source


def test_root_menu_contains_no_exit_label_or_exit_icon() -> None:
    source = (ROOT / "gba/src/ui_shell.cpp").read_text()
    root = source.split("void UiShell::_show_root_menu", 1)[1].split("void UiShell::", 1)[0]
    assert "menu_exit_icon" not in root
    assert "localized_strings[language][22]" not in root


def test_main_uses_session_foreground_dispatch() -> None:
    text = (ROOT / "gba/src/main.cpp").read_text()
    assert "switch(session.foreground())" in text
    assert "controller.set_suspended_session_available(session.has_suspended())" in text
    assert "else if(quick_game.active())" not in text
    assert "UiAction::ResumeSuspended" in text


def test_legacy_game_request_bridge_is_removed() -> None:
    header = (ROOT / "gba/include/tb/ui_controller.h").read_text()
    source = (ROOT / "gba/src/ui_controller.cpp").read_text()
    assert "GameRequest" not in header
    assert "pending_game_request" not in source
    assert "clear_game_request" not in source


def test_name_entry_text_change_invalidates_ui_shell_cache() -> None:
    header = (ROOT / "gba/include/tb/ui_shell.h").read_text()
    source = (ROOT / "gba/src/ui_shell.cpp").read_text()
    assert "_last_name_signature" in header
    update = _function_body(source, "void UiShell::update(const UiController& controller, const SaveData& save, const InputFrame& input)")
    assert "controller.name_entry()" in update
    assert "name_signature != _last_name_signature" in update
