extends Node2D

## Minimal networking + click-to-move for the first multiplayer milestone.
## The server is authoritative; this script only sends intentions and
## renders whatever state the server reports back.

const SERVER_URL := "ws://127.0.0.1:43594"
const PROTOCOL_VERSION := 1
const WORLD_WIDTH := 100
const WORLD_HEIGHT := 100
const TILE_SIZE := 32
const PLAYER_SIZE := Vector2(16, 16)
const MAX_CHAT_LINES := 10
const MIN_ZOOM := 0.5
const MAX_ZOOM := 3.0
const ZOOM_STEP := 0.25

var _socket := WebSocketPeer.new()
var _handshake_sent := false
var _my_id := ""
var _players: Dictionary = {} # id (String) -> ColorRect
var _chat_log: RichTextLabel
var _chat_input: LineEdit
var _camera: Camera2D


func _ready() -> void:
	_camera = Camera2D.new()
	_camera.position = Vector2(TILE_SIZE / 2.0, TILE_SIZE / 2.0)
	add_child(_camera)
	_setup_chat_ui()
	_socket.connect_to_url(SERVER_URL)
	queue_redraw()


func _draw() -> void:
	var world_size := Vector2(WORLD_WIDTH * TILE_SIZE, WORLD_HEIGHT * TILE_SIZE)
	draw_rect(Rect2(Vector2.ZERO, world_size), Color("263238"), true)
	for x in range(WORLD_WIDTH + 1):
		var pixel_x := x * TILE_SIZE
		draw_line(Vector2(pixel_x, 0), Vector2(pixel_x, world_size.y), Color("455a64"))
	for y in range(WORLD_HEIGHT + 1):
		var pixel_y := y * TILE_SIZE
		draw_line(Vector2(0, pixel_y), Vector2(world_size.x, pixel_y), Color("455a64"))


func _process(_delta: float) -> void:
	_socket.poll()
	var state := _socket.get_ready_state()

	if state == WebSocketPeer.STATE_OPEN:
		if not _handshake_sent:
			_handshake_sent = true
			_send({"type": "HELLO", "protocol_version": PROTOCOL_VERSION})
		while _socket.get_available_packet_count() > 0:
			_handle_message(_socket.get_packet().get_string_from_utf8())
	elif state == WebSocketPeer.STATE_CLOSED:
		set_process(false)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		var destination := Vector2i((get_global_mouse_position() / TILE_SIZE).floor())
		if destination.x >= 0 and destination.x < WORLD_WIDTH and destination.y >= 0 and destination.y < WORLD_HEIGHT:
			_send({"type": "MOVE_REQUEST", "x": destination.x, "y": destination.y})
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			_set_zoom(_camera.zoom.x + ZOOM_STEP)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			_set_zoom(_camera.zoom.x - ZOOM_STEP)


func _set_zoom(value: float) -> void:
	var clamped_zoom := clampf(value, MIN_ZOOM, MAX_ZOOM)
	_camera.zoom = Vector2(clamped_zoom, clamped_zoom)


func _send(message: Dictionary) -> void:
	_socket.send_text(JSON.stringify(message))


func _on_chat_text_submitted(new_text: String) -> void:
	var text := new_text.strip_edges()
	if text.is_empty():
		return
	_send({"type": "CHAT_SEND", "text": text})
	_chat_input.clear()


func _handle_message(packet: String) -> void:
	var parsed = JSON.parse_string(packet)
	if typeof(parsed) != TYPE_DICTIONARY:
		return

	match parsed.get("type", ""):
		"HELLO_ACK":
			if parsed.get("accepted", false):
				_my_id = parsed.get("id", "")
			else:
				push_warning("Server rejected HELLO: %s" % parsed.get("reason", ""))
		"PLAYER_SPAWN":
			_upsert_player(parsed["id"], parsed["x"], parsed["y"])
		"PLAYER_MOVED":
			_upsert_player(parsed["id"], parsed["x"], parsed["y"])
		"PLAYER_DESPAWN":
			_despawn_player(parsed["id"])
		"CHAT_BROADCAST":
			_append_chat_line("[%s] %s" % [parsed["from"], parsed["text"]])


func _upsert_player(id: String, x: float, y: float) -> void:
	if not _players.has(id):
		var node := ColorRect.new()
		node.size = PLAYER_SIZE
		node.color = Color.RED if id == _my_id else Color.CORNFLOWER_BLUE
		add_child(node)
		_players[id] = node

	var node: ColorRect = _players[id]
	node.position = Vector2((x + 0.5) * TILE_SIZE, (y + 0.5) * TILE_SIZE) - PLAYER_SIZE / 2.0
	if id == _my_id:
		_camera.position = node.position + PLAYER_SIZE / 2.0


func _despawn_player(id: String) -> void:
	if _players.has(id):
		_players[id].queue_free()
		_players.erase(id)


func _setup_chat_ui() -> void:
	var ui := CanvasLayer.new()
	add_child(ui)
	_chat_log = RichTextLabel.new()
	_chat_log.position = Vector2(12, 12)
	_chat_log.size = Vector2(420, 180)
	_chat_log.fit_content = false
	_chat_log.scroll_active = true
	_chat_log.selection_enabled = false
	ui.add_child(_chat_log)

	_chat_input = LineEdit.new()
	_chat_input.position = Vector2(12, 200)
	_chat_input.size = Vector2(420, 28)
	_chat_input.placeholder_text = "Press Enter to chat"
	_chat_input.text_submitted.connect(_on_chat_text_submitted)
	ui.add_child(_chat_input)


func _append_chat_line(line: String) -> void:
	var lines := PackedStringArray(_chat_log.text.split("\n", false))
	lines.append(line)
	while lines.size() > MAX_CHAT_LINES:
		lines.remove_at(0)
	_chat_log.text = "\n".join(lines)
	_chat_log.scroll_to_line(lines.size())
