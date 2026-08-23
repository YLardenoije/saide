extends Node2D

## Minimal networking + click-to-move for the first multiplayer milestone.
## The server is authoritative; this script only sends intentions and
## renders whatever state the server reports back.

const SERVER_URL := "ws://127.0.0.1:43594"
const PROTOCOL_VERSION := 1
const PLAYER_SIZE := Vector2(16, 16)

var _socket := WebSocketPeer.new()
var _handshake_sent := false
var _my_id := ""
var _players: Dictionary = {} # id (String) -> ColorRect


func _ready() -> void:
	_socket.connect_to_url(SERVER_URL)


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
		_send({"type": "MOVE_REQUEST", "x": event.position.x, "y": event.position.y})


func _send(message: Dictionary) -> void:
	_socket.send_text(JSON.stringify(message))


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


func _upsert_player(id: String, x: float, y: float) -> void:
	if not _players.has(id):
		var node := ColorRect.new()
		node.size = PLAYER_SIZE
		node.color = Color.RED if id == _my_id else Color.CORNFLOWER_BLUE
		add_child(node)
		_players[id] = node

	var node: ColorRect = _players[id]
	node.position = Vector2(x, y) - PLAYER_SIZE / 2.0


func _despawn_player(id: String) -> void:
	if _players.has(id):
		_players[id].queue_free()
		_players.erase(id)
