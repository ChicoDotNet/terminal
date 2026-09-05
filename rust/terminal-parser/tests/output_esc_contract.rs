use terminal_parser::output_engine::{
    DeviceAttributesKind, LineFeedType, LineRendition, OutputAction, OutputStateMachineEngine,
    TermDispatch,
};
use terminal_parser::state_machine::{StateMachineEngine, VtId};

#[derive(Debug, Default)]
struct RecordingDispatch {
    actions: Vec<OutputAction>,
}

impl TermDispatch for RecordingDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.actions.push(action);
    }
}

fn assert_esc(id: &str, expected: Option<OutputAction>) {
    let mut engine = OutputStateMachineEngine::new(RecordingDispatch::default());
    assert!(engine.action_esc_dispatch(VtId::from_ascii(id)));
    let expected = expected.into_iter().collect::<Vec<_>>();
    assert_eq!(engine.dispatch().actions.as_slice(), expected.as_slice());
}

#[test]
fn output_esc_dispatch_replays_the_cpp_contract() {
    let cases = [
        ("\\", None),
        ("6", Some(OutputAction::BackIndex)),
        ("7", Some(OutputAction::CursorSaveState)),
        ("8", Some(OutputAction::CursorRestoreState)),
        ("9", Some(OutputAction::ForwardIndex)),
        ("=", Some(OutputAction::SetKeypadMode(true))),
        (">", Some(OutputAction::SetKeypadMode(false))),
        (
            "E",
            Some(OutputAction::LineFeed(LineFeedType::WithReturn)),
        ),
        (
            "D",
            Some(OutputAction::LineFeed(LineFeedType::WithoutReturn)),
        ),
        ("M", Some(OutputAction::ReverseLineFeed)),
        ("H", Some(OutputAction::HorizontalTabSet)),
        (
            "Z",
            Some(OutputAction::DeviceAttributes(DeviceAttributesKind::Primary)),
        ),
        ("c", Some(OutputAction::HardReset)),
        ("N", Some(OutputAction::SingleShift(2))),
        ("O", Some(OutputAction::SingleShift(3))),
        ("n", Some(OutputAction::LockingShift(2))),
        ("o", Some(OutputAction::LockingShift(3))),
        ("~", Some(OutputAction::LockingShiftRight(1))),
        ("}", Some(OutputAction::LockingShiftRight(2))),
        ("|", Some(OutputAction::LockingShiftRight(3))),
        (" 7", Some(OutputAction::AcceptC1Controls(true))),
        (" F", Some(OutputAction::SendC1Controls(false))),
        (" G", Some(OutputAction::SendC1Controls(true))),
        (" L", Some(OutputAction::AnnounceCodeStructure(1))),
        (" M", Some(OutputAction::AnnounceCodeStructure(2))),
        (" N", Some(OutputAction::AnnounceCodeStructure(3))),
        (
            "#3",
            Some(OutputAction::SetLineRendition(LineRendition::DoubleHeightTop)),
        ),
        (
            "#4",
            Some(OutputAction::SetLineRendition(LineRendition::DoubleHeightBottom)),
        ),
        (
            "#5",
            Some(OutputAction::SetLineRendition(LineRendition::SingleWidth)),
        ),
        (
            "#6",
            Some(OutputAction::SetLineRendition(LineRendition::DoubleWidth)),
        ),
        ("#8", Some(OutputAction::ScreenAlignmentPattern)),
    ];

    for (id, expected) in cases {
        assert_esc(id, expected);
    }
}

#[test]
fn output_esc_charset_dispatch_replays_the_cpp_contract() {
    let cases = [
        ("%G", OutputAction::DesignateCodingSystem(u64::from(b'G'))),
        (
            "(B",
            OutputAction::Designate94Charset {
                slot: 0,
                charset: u64::from(b'B'),
            },
        ),
        (
            ")B",
            OutputAction::Designate94Charset {
                slot: 1,
                charset: u64::from(b'B'),
            },
        ),
        (
            "*B",
            OutputAction::Designate94Charset {
                slot: 2,
                charset: u64::from(b'B'),
            },
        ),
        (
            "+B",
            OutputAction::Designate94Charset {
                slot: 3,
                charset: u64::from(b'B'),
            },
        ),
        (
            "-A",
            OutputAction::Designate96Charset {
                slot: 1,
                charset: u64::from(b'A'),
            },
        ),
        (
            ".A",
            OutputAction::Designate96Charset {
                slot: 2,
                charset: u64::from(b'A'),
            },
        ),
        (
            "/A",
            OutputAction::Designate96Charset {
                slot: 3,
                charset: u64::from(b'A'),
            },
        ),
    ];

    for (id, expected) in cases {
        assert_esc(id, Some(expected));
    }
}
