// This file reads environment variables from an .env file and validates them with zod
// It exports the validated environment variables as an object to be used throughout the server

import { z } from "zod";
import "dotenv/config";

// .env schema
const serverEnvSchema = z.object({
  CONVEX_DEPLOYMENT: z.string().min(1, "CONVEX_DEPLOYMENT is required"),
  NODE_ENV: z
    .enum(["development", "production"], {
      message: "NODE_ENV must be either 'development' or 'production'",
    })
    .default("development"),
});

// Read .env, validate and export it
export const serverEnv = serverEnvSchema.parse(process.env);
export type ServerEnv = z.infer<typeof serverEnvSchema>;
