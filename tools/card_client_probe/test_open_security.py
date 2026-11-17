"""Untrusted ACAR clients exercise the public TLS protocol with test-only credentials."""

import time
import unittest

from test_integration import AuthorityHarness, decode, receive, request

POLL = 2
DRAW = 3


def exchange(peer, frame):
    peer.sendall(frame)
    return receive(peer)


class OpenSecurityTests(AuthorityHarness):
    def running_game(self):
        self.start(audit=False)
        return self.all_seats()

    def views(self, peers):
        return [exchange(peer, request(POLL)) for peer in peers]

    def reconnect(self, peers, seat, observed_revision):
        peers[seat].close()
        observer = peers[(seat + 1) % len(peers)]
        deadline = time.monotonic() + 2
        while self.poll(observer)["rev"] == observed_revision:
            self.assertLess(time.monotonic(), deadline, "server did not observe disconnect")
            time.sleep(0.04)
        peers[seat], state = self.connect(seat)
        return state

    def test_draw_reveals_only_the_recipients_resulting_hand(self):
        peers, before = self.running_game()
        actor = before[0]["turn"]
        victim = before[0]["victim"]
        self.assertNotEqual(actor, victim)
        expected_hands = [view["own"].copy() for view in before]
        drawn = expected_hands[victim].pop(0)
        actor_hand = expected_hands[actor]
        partner = next(
            (index for index, card in enumerate(actor_hand)
             if drawn != 52 and card != 52 and card % 13 == drawn % 13),
            None,
        )
        if partner is None:
            actor_hand.append(drawn)
        else:
            actor_hand.pop(partner)

        actor_view = before[actor]
        draw = request(DRAW, 0, actor_view["seq"], actor_view["rev"])
        answer = decode(exchange(peers[actor], draw))
        self.assertEqual(answer["error"], 0)
        self.assertEqual(answer["rev"], actor_view["rev"] + 1)

        counts = [len(hand) for hand in expected_hands]
        for seat, raw in enumerate(self.views(peers)):
            with self.subTest(seat=seat):
                view = decode(raw)
                self.assertEqual(view["seat"], seat)
                self.assertEqual(view["rev"], answer["rev"])
                self.assertEqual(view["counts"], counts)
                self.assertEqual(view["discarded"], before[seat]["discarded"] + (partner is not None))
                expected = bytes(expected_hands[seat])
                self.assertEqual(raw[40:], expected.ljust(24, b"\0"))

    def test_accepted_draw_cannot_be_replayed_across_reconnection(self):
        peers, views = self.running_game()
        actor = views[0]["turn"]
        actor_view = views[actor]
        draw = request(DRAW, 0, actor_view["seq"], actor_view["rev"])
        accepted = decode(exchange(peers[actor], draw))
        self.assertEqual(accepted["error"], 0)
        reconnected = self.reconnect(peers, actor, accepted["rev"])
        self.assertEqual(reconnected["seq"], accepted["seq"])
        self.assertGreater(reconnected["rev"], accepted["rev"])
        baseline = self.views(peers)

        # Changing just the revision cannot make a consumed operation number valid again.
        repeated = request(DRAW, 0, actor_view["seq"], reconnected["rev"])
        for attack in (draw, repeated):
            result = decode(exchange(peers[actor], attack))
            self.assertEqual(result["error"], 4)
            self.assertEqual(result["seq"], reconnected["seq"])
            self.assertEqual(result["rev"], reconnected["rev"])
        self.assertEqual(self.views(peers), baseline)

        # A fresh operation number still cannot make an earlier game revision current.
        stale = request(DRAW, 0, reconnected["seq"], actor_view["rev"])
        result = decode(exchange(peers[actor], stale))
        self.assertEqual(result["error"], 3)
        self.assertEqual(result["seq"], reconnected["seq"] + 1)
        self.assertEqual(result["rev"], reconnected["rev"])
        after = self.views(peers)
        for seat, (old, current) in enumerate(zip(baseline, after)):
            with self.subTest(seat=seat):
                if seat == actor:
                    self.assertEqual(decode(current)["seq"], decode(old)["seq"] + 1)
                    self.assertEqual(current[:8] + current[16:], old[:8] + old[16:])
                else:
                    self.assertEqual(current, old)

    def test_authenticated_client_cannot_submit_an_authoritative_result(self):
        peers, views = self.running_game()
        actor = views[0]["turn"]
        observer = (actor + 1) % len(peers)
        baseline = self.views(peers)
        baseline_revision = views[actor]["rev"]

        # Send a forged server-shaped frame over a valid TLS session and seat identity.
        forged = bytearray(baseline[actor])
        forged[30] = observer  # Claim that another seat lost.
        forged[37] = 2  # Claim that the round is finished.
        peers[actor].sendall(forged)
        with self.assertRaises((EOFError, ConnectionError)):
            receive(peers[actor])

        restored = self.reconnect(peers, actor, baseline_revision)
        self.assertEqual(restored["rev"], baseline_revision + 2)
        after = self.views(peers)
        for seat, (old, current) in enumerate(zip(baseline, after)):
            with self.subTest(seat=seat):
                # A disconnect and reclaim account for the only state revision changes.
                self.assertEqual(current[:16] + current[24:], old[:16] + old[24:])

        # Positive control: a valid command in the same state is still accepted.
        answer = decode(exchange(peers[actor], request(DRAW, 0, restored["seq"], restored["rev"])))
        self.assertEqual(answer["error"], 0)
        self.assertEqual(answer["rev"], restored["rev"] + 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
