using Godot;



public partial class Main : Control
{
    public override void _Ready()
    {
        GD.Print("Soulbound initialized!");

        // Load the card scene.
        PackedScene cardScene =
            GD.Load<PackedScene>("res://scenes/CardView.tscn");

        // Create a card instance.
        Control card = cardScene.Instantiate<Control>();

        // Find the middle unit zone.
        Control unitZone =
            GetNode<Control>("Battlefield/UnitRow/UnitZone");

        // Place the card into the zone.
        unitZone.AddChild(card);


		// Reset the card's anchors and offsets.
		card.SetAnchorsAndOffsetsPreset(
			Control.LayoutPreset.FullRect
		);
    }
}
