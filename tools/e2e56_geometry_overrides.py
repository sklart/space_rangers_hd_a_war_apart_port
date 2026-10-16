"""Reuse identical Extended trig results within a single geometry calculation."""


def apply(text: str) -> str:
    for method, expressions in [
        ('AdvanceSphericalBearingState', ['Cos(OldPolar)', 'Cos(ArcAngle)', 'Sin(OldPolar)', 'Sin(ArcAngle)', 'Cos(OldBearing)']),
        ('ComputeSphericalBearingAndDistance', ['Cos(TargetPolar)', 'Cos(SourcePolar)', 'Sin(TargetPolar)', 'Sin(SourcePolar)', 'Cos(LongitudeDelta)']),
    ]:
        start = text.index(f'    void {method}(')
        end = text.index('\n    }', start) + len('\n    }')
        body = text[start:end]
        declarations = []
        for i, expression in enumerate(expressions):
            call = 'System::' + expression
            count = body.count(call)
            if count < 2:
                raise RuntimeError(f'{method}: repeated expression changed: {call}')
            name = f'e2e_trig_{i}'
            # Keep the first call in its original evaluation position, at full
            # Extended precision. Later uses observe the identical operands.
            first = body.index(call)
            body = body[:first] + f'({name} = {call})' + body[first + len(call):].replace(call, name)
            declarations.append(f'        pas::Extended {name}{{}};')
        opening = body.index(' {') + 2
        body = body[:opening] + '\n' + '\n'.join(declarations) + body[opening:]
        text = text[:start] + body + text[end:]
    return text
