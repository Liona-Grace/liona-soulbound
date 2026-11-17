# Liona Soulbound

Vanilla Core/CLI validation: [Milestone 3 report](docs/validation/milestone-3.md)
includes confirmed rules, deterministic simulation results and unresolved decisions.

Project gồm C++20 core độc lập và giao diện Godot C#.

- `core/`: trạng thái, luật trận đấu, JSON loader và combat; namespace `soulbound`.
- `cli/`: simulator terminal hai người chơi; xem [lệnh và ví dụ CLI](cli/README.md).
- `data/game_config.json`: cấu hình HP, Hand, Board và Deck lúc khởi tạo.
- `data/cards.json`: định nghĩa hai Unit vanilla Knight và Guard.
- `tests/core/`: kiểm thử tự động, chạy bằng CTest, không cần GUI.
- `tests/cli/`: kiểm thử parsing/dispatch và kịch bản chơi trọn trận.
- `adapters/`: dành cho lớp kết nối core với Godot trong tương lai.
- `godot/`: project Godot C# với scenes, scripts và assets riêng.

## Build và kiểm thử

Cần CMake 3.16+ và compiler hỗ trợ C++20. JSON được xử lý bằng nlohmann/json
3.12.0, lưu sẵn tại `core/third-party/nlohmann/json.hpp` (giấy phép MIT trong header).
Không tải dependency khi build.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/cli/soulbound_cli
```

Target thư viện: `soulbound_core` (alias `soulbound::core`). Có thể tắt test bằng
`-DBUILD_TESTING=OFF`.

## Deck Management

Core cung cấp CardDatabase, DeckDefinition, DeckValidator, DeckStorage và
DeckManager. Deck JSON nằm trong `data/decks/`; lưu được Deck nháp, nhưng chỉ
Deck đủ `GameConfig.deckSize` mới bắt đầu trận. Mỗi Card ID tối đa
`GameConfig.maxCopiesPerCard` bản sao (mặc định 3), tính cả các entry trùng ID. Xem [API và schema Deck](docs/deck-management.md).

```sh
./build/cli/soulbound_cli --list-decks
./build/cli/soulbound_cli data starter-knight starter-guard
```

## Cấu hình và dữ liệu

Chỉnh `data/game_config.json` để cấu hình trận đấu:

```json
{
  "startingHP": 20,
  "startingHandSize": 4,
  "boardSize": 6,
  "deckSize": 30
}
```

`Game::fromFiles(configPath, cardsPath, seed)` nạp và kiểm tra cấu hình **trước**
khi nạp CardDatabase. Nếu cấu hình lỗi, card database chưa được đọc.
`JsonLoader::loadGameConfig(path)` cũng dùng độc lập được.
Cần đủ bốn trường số nguyên; HP, boardSize, deckSize phải dương;
startingHandSize không âm và không vượt deckSize. Trường thiếu, trùng, lạ hoặc
sai kiểu đều gây `std::runtime_error`. Trường tùy chọn `maxCopiesPerCard` phải
là số nguyên dương; file cũ không có trường này dùng mặc định 3.
`GameConfig` vẫn có mặc định 20/4/6/30 cho các mô phỏng cấu hình trực tiếp bằng C++.

Mỗi người rút 4 lá từ Deck lúc khởi tạo, còn 26 lá. Mọi tham số trên đều có thể cấu hình.
Deck đầu vào là danh sách ID định nghĩa thẻ, cho phép lặp lại ID.

| ID | Tên | ATK | HP gốc / phí triệu hồi |
| --- | --- | --- | --- |
| `knight` | Knight | 5 | 8 |
| `guard` | Guard | 3 | 6 |

`JsonLoader::loadCards(path)` trả về `CardDatabase`, ném
`std::runtime_error` nếu không đọc được file, JSON sai hoặc dữ liệu không hợp lệ.
Schema hiện chỉ hỗ trợ Unit vanilla:

```json
{
  "cards": [
    { "id": "knight", "name": "Knight", "atk": 5, "hp": 8 }
  ]
}
```

Mỗi card phải có đủ bốn trường trên, ID/tên không rỗng, ATK nguyên không âm,
HP nguyên dương. Loader từ chối ID trùng, trường trùng, trường lạ và sai kiểu.

## API và luật hiện tại

Ví dụ tạo trận đấu với mỗi Deck gồm 15 Knight và 15 Guard:

```cpp
#include <soulbound/JsonLoader.hpp>
#include <soulbound/Game.hpp>

int main() {
    auto game = soulbound::Game::fromFiles(
        "data/game_config.json", "data/cards.json", 42);
    const auto deckSize = game.getConfig().deckSize;
    soulbound::Game::Deck deck(deckSize / 2, "knight");
    deck.insert(deck.end(), deckSize - deck.size(), "guard");
    if (game.start(deck, deck) != soulbound::ActionError::None) return 1;

    const auto instanceId = game.getState().players[0].hand.front().id;
    if (game.summonUnit(0, instanceId, 0) != soulbound::ActionError::None) return 1;
    return 0;
}
```

- PlayerId là `0` hoặc `1`; Player `0` đi trước. Zone đánh số từ `0`.
- Mỗi lượt bắt đầu ở `Phase::Main`. `beginBattle()` chuyển sang Battle và khóa
  triệu hồi/rút bài cho đến lượt sau. `endTurn()` đổi Player, tăng số lượt và
  xóa cờ đã chiến đấu của các Unit. Có thể kết thúc lượt ngay từ Main.
- `drawCard(player)` rút một lá trong Main của Player hiện tại. Hiện không tự
  rút bài khi chuyển lượt. Mỗi lượt chỉ được rút thường một lần, đúng một lá;
  `hasDrawnThisTurn` được reset khi chuyển lượt. Rút thất bại không tiêu tốn quyền này.
  Giới hạn này dành cho hành động rút thường, không dành cho hiệu ứng card sau này.
- `summonUnit(player, instanceId, zone)` chuyển đúng instance từ Hand vào Zone
  trống, trả HP bằng HP gốc. Được trả vừa đủ xuống 0; không được trả thiếu.
- `attackUnit(attackerId, defenderId)` chỉ nhắm Unit đối thủ.
  Attacker đánh trước; Defender chỉ phản công khi sống sót.
- Một Unit chỉ chủ động tấn công một lần mỗi lượt, được đánh dấu `hasAttacked`.
  Defender có thể bị tấn công nhiều lần và phản công khi sống sót; phòng thủ
  không tiêu tốn quyền chủ động tấn công.
- `attackPlayer(attackerId, player)` chỉ hợp lệ nếu đối thủ không có Unit trên Board.
  Cũng tiêu thụ lần chiến đấu của Attacker.
- Lượt đầu tiên **của trận** không được tấn công. Từ lượt thứ hai, Unit vừa triệu hồi
  có thể chiến đấu ngay khi chuyển sang Battle.
- Cả đòn đánh và phản công chỉ truyền sát thương dư khi Unit nhận đòn bị chết.
  Unit chết được chuyển vào Tomb trước khi kiểm tra thắng thua.
- Player HP không giảm dưới 0 khi nhận sát thương; Player chỉ thua khi
  **HP = 0 và Board không còn Unit**.
  Nếu cả hai cùng thỏa điều kiện này thì hòa. `Game::evaluateOutcome(state)`
  cũng cho phép đánh giá thuần dữ liệu, bao gồm tình huống đồng thời này.
- Unit hồi máu tối đa bằng HP gốc trong `CardDefinition`. Player hồi máu
  không giới hạn bởi HP ban đầu. Tomb không hồi sinh.
- `damagePlayer`, `damageUnit`, `healPlayer`, `healUnit`, `moveToTomb` là API
  thao tác nội bộ engine; adapter cần phân biệt chúng với hành động của người chơi.
  Sát thương qua `damageUnit` không truyền dư; truyền dư thuộc logic combat.
- Deck rỗng không gây thua. `drawCard` trả `ActionError::EmptyDeck` và không đổi
  trạng thái; có `TODO` để xem xét lại luật này sau.

Hành động trả `ActionError`; hành động không hợp lệ không thay đổi trạng thái.
Sau khi trận kết thúc, các hành động bị từ chối. `start()` có thể bắt đầu lại trận;
`reset()` đưa về trạng thái chưa bắt đầu. InstanceId không lặp lại giữa các lần
khởi tạo của cùng một đối tượng Game.

Game giữ bản sao riêng của CardDatabase và chỉ cung cấp trạng thái qua
`const GameState&`. GameState chỉ chứa dữ liệu; việc thay đổi trận đấu đi qua
logic core. Mỗi instance chỉ nằm ở một trong Deck, Hand, Board hoặc Tomb.
Seed có thể truyền vào constructor để tái hiện xáo bài trong simulator/test.

## Chạy giao diện

Mở `godot/project.godot` bằng Godot .NET tương thích với project hiện tại
(Godot.NET.Sdk 4.7.2 trong file `.csproj`). Main scene là `godot/scenes/Main.tscn`.
Solution C# nằm ở `godot/liona-soulbound.sln`.

Core hiện chưa kết nối với giao diện qua adapter. Đường dẫn `res://` tính từ
thư mục `godot/`; adapter hoặc bước đóng gói sẽ xử lý việc truy cập `data/` sau.
