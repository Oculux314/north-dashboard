import { defineSchema, defineTable } from "convex/server";
import { Infer, v } from "convex/values";

const schema = {
  logs: defineTable({
    timestamp: v.number(),
    voltage: v.number(),
    current: v.number(),
  }).index("by_timestamp", ["timestamp"]),
};

export default defineSchema(schema);
export type Log = Infer<typeof schema.logs.validator>;
