"""Build deterministic, human-readable walkthroughs of the pump-station slice."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent


class Route:
    def __init__(self, mission=0, site=0, kit=0):
        self.events = []
        self.frame = 0
        for _ in range(kit):
            self.key(9, 1)
        for _ in range(site):
            self.key(2, 1)
        for _ in range(mission):
            self.key(1, 1)
        self.key(6, 1)
        self.x, self.z = 18.7, 3.3
        self.checkpoints = {}
        self.sneaking = False

    def key(self, code, duration):
        self.events.append(dict(frame=self.frame, type="action", code=code, pressed=True))
        self.events.append(dict(frame=self.frame + duration, type="action", code=code, pressed=False))
        self.frame += duration + 1

    def go(self, x, z):
        tx, tz = (x + .5) * 2.2, (z + .5) * 2.2
        dx, dz = tx - self.x, tz - self.z
        speed = .045 if self.sneaking else .075
        if abs(dx) > .04:
            ticks = round(abs(dx) / speed)
            self.key(9 if dx > 0 else 8, ticks)
            self.x += ticks * speed * (1 if dx > 0 else -1)
        if abs(dz) > .04:
            ticks = round(abs(dz) / speed)
            self.key(2 if dz > 0 else 5, ticks)
            self.z += ticks * speed * (1 if dz > 0 else -1)

    def sneak(self, enabled):
        self.events.append(dict(frame=self.frame, type="action", code=7, pressed=enabled))
        self.frame += 1
        self.sneaking = enabled

    def use(self, name):
        self.key(6, 1)
        self.frame += 39
        self.checkpoints[name] = self.frame

    def save(self, name):
        (ROOT / name).write_text(json.dumps({"events": self.events}, indent=2) + "\n")
        return {**self.checkpoints, "end": self.frame + 2}


def power_route(relay=False, mission=0, site=0):
    route = Route(mission, site)
    route.go(7, 1)
    route.use("fuse")
    route.go(8, 1)
    route.go(8, 2)
    route.go(12, 2)
    route.go(12, 4)
    route.use("power")
    if relay:
        route.use("relay")
    route.go(12, 2)
    route.go(8, 2)
    route.go(8, 6)
    route.use("pump")
    route.frame += 60
    route.checkpoints["draining"] = route.frame
    route.frame += 120
    route.checkpoints["drained"] = route.frame
    return route


def full_route(relay=False, return_home=True):
    route = power_route(relay)
    route.go(8, 2)
    route.go(2, 2)
    route.go(2, 8)
    route.use("west")
    route.go(2, 2)
    route.go(14, 2)
    route.go(14, 8)
    route.use("east")
    route.go(14, 14)
    if relay:
        # Observe a patrol pass from the east entrance, then cross behind it.
        route.frame += 180
        route.sneak(True)
    route.go(8, 14)
    if relay:
        route.sneak(False)
    route.use("record")
    if not return_home:
        return route
    if relay:
        route.go(8, 10)
        route.go(10, 10)
        route.go(10, 6)
        route.go(8, 6)
        route.go(8, 1)
    else:
        route.go(14, 14)
        route.go(14, 2)
        route.go(8, 2)
        route.go(8, 1)
    return route


if __name__ == "__main__":
    checkpoints = {}
    for name, route in [("power-slice.json", power_route()),
                        ("full-route.json", full_route()),
                        ("relay-shortcut.json", full_route(True))]:
        checkpoints[name] = route.save(name)
    chart = Route()
    chart.go(8, 2)
    chart.go(4, 2)
    chart.go(4, 4)
    chart.use("chart")
    checkpoints["chart-route.json"] = chart.save("chart-route.json")
    missing = Route()
    missing.go(8, 2)
    missing.go(12, 2)
    missing.go(12, 4)
    missing.use("attempt")
    checkpoints["missing-fuse.json"] = missing.save("missing-fuse.json")
    unpowered = Route()
    unpowered.go(8, 6)
    unpowered.use("attempt")
    checkpoints["unpowered-pump.json"] = unpowered.save("unpowered-pump.json")
    patrol = full_route(False, False)
    patrol.go(8, 13)
    patrol.key(0, 28)
    patrol.checkpoints["watch"] = patrol.frame
    patrol.frame += 900
    patrol.checkpoints["caught"] = patrol.frame
    patrol.use("retry")
    checkpoints["patrol-retry.json"] = patrol.save("patrol-retry.json")
    duct = power_route(mission=2)
    duct.go(8, 2)
    duct.go(4, 2)
    duct.go(4, 4)
    duct.use("chart")
    duct.go(4, 2)
    duct.go(8, 2)
    duct.go(8, 6)
    duct.go(6, 6)
    duct.go(6, 10)
    duct.checkpoints["mouth"] = duct.frame
    duct.sneak(True)
    duct.go(6, 11)
    duct.checkpoints["inside"] = duct.frame
    duct.go(5, 11)
    duct.sneak(False)
    duct.frame += 12
    duct.use("log")
    duct.sneak(True)
    duct.frame += 12
    duct.go(6, 11)
    duct.go(6, 10)
    duct.sneak(False)
    duct.go(6, 6)
    duct.go(8, 6)
    duct.go(8, 1)
    checkpoints["service-duct.json"] = duct.save("service-duct.json")
    submerged = Route()
    submerged.go(8, 6)
    submerged.go(6, 6)
    submerged.go(6, 10)
    submerged.checkpoints["mouth"] = submerged.frame
    submerged.sneak(True)
    submerged.go(6, 11)
    checkpoints["submerged-duct.json"] = submerged.save("submerged-duct.json")
    salvage = power_route()
    salvage.go(10, 6)
    salvage.go(10, 10)
    salvage.go(8, 10)
    salvage.sneak(True)
    salvage.go(8, 13)
    salvage.go(11, 13)
    salvage.go(11, 11)
    salvage.use("east_log")
    salvage.use("salvage")
    salvage.key(3, 1)
    salvage.use("empty_cabinet")
    checkpoints["salvage-route.json"] = salvage.save("salvage-route.json")
    for site, name in [(1, "patrol-loop.json"), (2, "patrol-scan.json")]:
        watch = power_route(site=site)
        watch.go(10, 6)
        watch.go(10, 10)
        watch.go(8, 10)
        watch.sneak(True)
        watch.go(8, 12)
        if site == 2:
            watch.frame += 180
        checkpoints[name] = watch.save(name)
    battery = Route(mission=1, site=4, kit=2)
    battery.go(8, 6)
    battery.use("battery_pump")
    battery.frame += 180
    battery.go(6, 6)
    battery.go(6, 8)
    battery.use("wrench_hatch")
    battery.go(2, 8)
    battery.use("west")
    battery.go(6, 8)
    battery.go(6, 6)
    battery.go(8, 6)
    battery.go(8, 1)
    checkpoints["battery-wrench-return.json"] = battery.save("battery-wrench-return.json")
    (ROOT / "checkpoints.json").write_text(json.dumps(checkpoints, indent=2) + "\n")
