#!nqp
use MASTTesting;

plan(1);

mast_frame_output_is(-> $frame, @ins, $cu {
        my $r0 := local($frame, int);
        my $r1 := local($frame, int);
        my $r2 := local($frame, str);
        my $r3 := local($frame, NQPMu);
        my $l_loop := label('loop');
        my $l_next := label('next');
        my $l_end  := label('end');
        
        op(@ins, 'const_i64', $r0, ival(0));
        
        my @loop_ins;
        nqp::push(@loop_ins, $l_loop);
        op(@loop_ins, 'inc_i', $r0);
        op(@loop_ins, 'const_i64', $r1, ival(3));
        op(@loop_ins, 'ne_i', $r1, $r0, $r1);
        op(@loop_ins, 'if_i', $r1, $l_next);
        op(@loop_ins, 'throwcatdyn', $r3, ival($HandlerCategory::last));
        nqp::push(@loop_ins, $l_next);
        op(@loop_ins, 'say_i', $r0);
        op(@loop_ins, 'goto', $l_loop);
        
        nqp::push(@ins, MAST::HandlerScope.new(
            :instructions(@loop_ins),
            :category_mask($HandlerCategory::last),
            :action($HandlerAction::unwind_and_goto),
            :goto($l_end)
        ));
        nqp::push(@ins, $l_end);
        op(@ins, 'const_s', $r2, sval('At program end'));
        op(@ins, 'say_s', $r2);
        op(@ins, 'return');
    },
    "1\n2\nAt program end\n",
    "last control exception worked within block");
